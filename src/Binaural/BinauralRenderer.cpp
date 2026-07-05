#include <SpatialCore/Binaural/BinauralRenderer.h>
#include <cstring>

namespace spatialcore
{

//==============================================================================
// BinauralRenderer implementation -- manages HRTF convolver banks
//==============================================================================
void BinauralRenderer::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = maxBlockSize;

    // Pre-allocate to max(blockSize, 512) to cover typical IR lengths
    // and avoid audio-thread allocation in renderSourceBuffers / updateSourceHRIR.
    size_t preAllocSize = static_cast<size_t> (std::max (maxBlockSize, 512));
    convTmpL.resize (preAllocSize, 0.0f);
    convTmpR.resize (preAllocSize, 0.0f);
}

void BinauralRenderer::setProfile (int profileIndex)
{
    activeProfile = profileIndex;

    if (profileIndex == 0 || ! hrtfDatabase.isLoaded())
    {
        // Simple mode -- no convolution needed
        storedNormGain = 1.0f;
        storedIRLength = 0;
        itdActive = false;
        for (int i = 0; i < MAX_SOURCES; ++i)
            sourceConvReady[i] = false;
        return;
    }

    // Enable ITD-free HRIR mode for all SOFA profiles.
    // ITD is extracted and applied separately for smooth crossfading.
    itdActive = true;

    int irLen = hrtfDatabase.getIRLength();
    storedIRLength = irLen;

    // =========================================================================
    // Compute cross-profile normalization gain.
    // Sample a few reference directions to measure this profile's energy level.
    // mysofa_loudness() normalises the FRONTAL HRIR so sumOfSquares = 2.0,
    // but off-axis HRIRs vary. We sample 6 directions (front, back, L, R, up, down)
    // to get a representative avgRMS, then scale to targetRMS = 1/sqrt(irLen).
    // =========================================================================
    constexpr float pi = juce::MathConstants<float>::pi;
    static const float refDirs[][2] = {
        { 0.0f, 0.0f },                                           // Front
        { pi, 0.0f },                                             // Back
        { pi * 0.5f, 0.0f },                                      // Left
        { -pi * 0.5f, 0.0f },                                     // Right
        { 0.0f, pi * 0.25f },                                     // Above-front
        { 0.0f, -pi * 0.25f }                                     // Below-front
    };
    static constexpr int NUM_REF_DIRS = 6;

    std::vector<float> tmpIRL (static_cast<size_t> (irLen)), tmpIRR (static_cast<size_t> (irLen));
    double totalEnergy = 0.0;

    for (int d = 0; d < NUM_REF_DIRS; ++d)
    {
        float delayL = 0.0f, delayR = 0.0f;
        hrtfDatabase.getInterpolatedHRIR (refDirs[d][0], refDirs[d][1],
                                         tmpIRL.data(), tmpIRR.data(), delayL, delayR);
        for (int n = 0; n < irLen; ++n)
        {
            totalEnergy += (double) tmpIRL[static_cast<size_t> (n)] * tmpIRL[static_cast<size_t> (n)];
            totalEnergy += (double) tmpIRR[static_cast<size_t> (n)] * tmpIRR[static_cast<size_t> (n)];
        }
    }

    const float targetRMS = 1.0f / std::sqrt ((float) irLen);
    const double avgRMS   = std::sqrt (totalEnergy / (double) (NUM_REF_DIRS * 2 * irLen));
    storedNormGain = (avgRMS > 1e-8) ? (float) (targetRMS / avgRMS) : 1.0f;

    // =========================================================================
    // Prepare per-source convolvers (allocate FFT buffers for irLength).
    // HRIRs are loaded later per-source via updateSourceHRIR() in processBlock.
    // =========================================================================
    for (int i = 0; i < MAX_SOURCES; ++i)
    {
        sourceConvL[i].prepare (currentBlockSize, irLen);
        sourceConvR[i].prepare (currentBlockSize, irLen);
        sourceConvReady[i] = false;   // Force HRIR reload on next processBlock
        cachedSourceAz[i] = -999.0f;  // Invalidate cached positions
        cachedSourceEl[i] = -999.0f;

        // Clear stale ITD state to prevent transient on profile switch
        currentITDL[i] = 0.0f;
        currentITDR[i] = 0.0f;
        targetITDL[i] = 0.0f;
        targetITDR[i] = 0.0f;
        itdWritePos[i] = 0;
        std::memset (itdBufferL[i], 0, sizeof (itdBufferL[i]));
        std::memset (itdBufferR[i], 0, sizeof (itdBufferR[i]));
    }

    // Low-shelf bass compensation for MIT KEMAR.
    // KEMAR has a 24 dB deficit at 50 Hz and 6 dB at 100 Hz (measurement limitation).
    // Design a low-shelf biquad filter to boost post-convolution output.
    lfShelfActive = (profileIndex == 5);  // Only MIT KEMAR needs compensation
    if (lfShelfActive)
    {
        // Low-shelf: +12 dB at 200 Hz, Q=0.7 (gentle slope)
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            currentSampleRate, 200.0f, 0.7f, juce::Decibels::decibelsToGain (12.0f));
        lfShelfB[0] = coeffs->coefficients[0];
        lfShelfB[1] = coeffs->coefficients[1];
        lfShelfB[2] = coeffs->coefficients[2];
        lfShelfA[0] = 1.0f;  // a0 normalized
        lfShelfA[1] = coeffs->coefficients[3];
        lfShelfA[2] = coeffs->coefficients[4];

        // Clear filter state
        for (int i = 0; i < MAX_SOURCES; ++i)
        {
            lfShelfStateL[i][0] = lfShelfStateL[i][1] = 0.0f;
            lfShelfStateR[i][0] = lfShelfStateR[i][1] = 0.0f;
        }
    }

    DBG ("BinauralRenderer: Profile " + juce::String (profileIndex)
         + " loaded -- IR=" + juce::String (irLen)
         + ", normGain=" + juce::String (storedNormGain, 4)
         + ", lfShelf=" + juce::String (lfShelfActive ? "ON" : "OFF")
         + " (per-source direct binaural)");
}

void BinauralRenderer::updateSourceHRIR (int sourceIndex, float azRad, float elRad)
{
    if (sourceIndex < 0 || sourceIndex >= MAX_SOURCES || storedIRLength <= 0)
        return;

    // Great-circle angular distance threshold replaces independent
    // azimuth/elevation comparison.
    constexpr float THRESHOLD = 0.017f;  // ~1 degree in radians
    if (sourceConvReady[sourceIndex])
    {
        float cachedAz = cachedSourceAz[sourceIndex];
        float cachedEl = cachedSourceEl[sourceIndex];
        float dot = std::cos (elRad) * std::cos (cachedEl) * std::cos (azRad - cachedAz)
                  + std::sin (elRad) * std::sin (cachedEl);
        dot = juce::jlimit (-1.0f, 1.0f, dot);
        float angularDist = std::acos (dot);
        if (angularDist < THRESHOLD)
            return;
    }

    // Query HRTF at exact source direction (realtime-safe: KD-tree lookup, no malloc)
    float delayL = 0.0f, delayR = 0.0f;
    std::vector<float>& tmpL = convTmpL;  // Reuse work buffer (safe: not in render path here)
    std::vector<float>& tmpR = convTmpR;

    // Ensure work buffers are large enough for IR
    if ((int) tmpL.size() < storedIRLength)
    {
        jassertfalse;  // Audio thread allocation -- should have been pre-allocated in prepare()
        tmpL.resize (static_cast<size_t> (storedIRLength));
        tmpR.resize (static_cast<size_t> (storedIRLength));
    }

    // Use ITD-free HRIRs for smooth crossfading (no comb-filtering from ITD misalignment)
    if (itdActive)
        hrtfDatabase.getAlignedHRIR (azRad, elRad, tmpL.data(), tmpR.data(), delayL, delayR);
    else
        hrtfDatabase.getInterpolatedHRIR (azRad, elRad, tmpL.data(), tmpR.data(), delayL, delayR);

    // Apply cross-profile normalization
    for (int n = 0; n < storedIRLength; ++n)
    {
        tmpL[static_cast<size_t> (n)] *= storedNormGain;
        tmpR[static_cast<size_t> (n)] *= storedNormGain;
    }

    // Load into convolver (realtime-safe: in-place FFT in pre-allocated buffers)
    sourceConvL[sourceIndex].setIR (tmpL.data(), storedIRLength);
    sourceConvR[sourceIndex].setIR (tmpR.data(), storedIRLength);

    // Store ITD target for smooth per-sample interpolation in renderSourceBuffers
    if (itdActive)
    {
        targetITDL[sourceIndex] = delayL;
        targetITDR[sourceIndex] = delayR;
    }

    cachedSourceAz[sourceIndex] = azRad;
    cachedSourceEl[sourceIndex] = elRad;
    sourceConvReady[sourceIndex] = true;
}

void BinauralRenderer::renderSourceBuffers (const float* const* sourceBufs,
                                             const bool* sourceEnabled,
                                             int numSources, int numSamples,
                                             float* outL, float* outR)
{
    // Zero output
    std::memset (outL, 0, sizeof (float) * static_cast<size_t> (numSamples));
    std::memset (outR, 0, sizeof (float) * static_cast<size_t> (numSamples));

    if (convTmpL.size() < static_cast<size_t> (numSamples))
    {
        jassertfalse;  // Audio thread allocation -- should have been pre-allocated in prepare()
        convTmpL.resize (static_cast<size_t> (numSamples));
        convTmpR.resize (static_cast<size_t> (numSamples));
    }

    for (int src = 0; src < numSources && src < MAX_SOURCES; ++src)
    {
        if (! sourceEnabled[src] || ! sourceConvReady[src])
            continue;

        // Convolve this source's accumulated signal with its L and R HRIRs
        sourceConvL[src].process (sourceBufs[src], convTmpL.data(), numSamples);
        sourceConvR[src].process (sourceBufs[src], convTmpR.data(), numSamples);

        // Low-shelf bass boost for KEMAR.
        if (lfShelfActive)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                // Transposed Direct Form II biquad -- left channel
                float xL = convTmpL[static_cast<size_t> (s)];
                float yL = lfShelfB[0] * xL + lfShelfStateL[src][0];
                lfShelfStateL[src][0] = lfShelfB[1] * xL - lfShelfA[1] * yL + lfShelfStateL[src][1];
                lfShelfStateL[src][1] = lfShelfB[2] * xL - lfShelfA[2] * yL;
                convTmpL[static_cast<size_t> (s)] = yL;

                // Right channel
                float xR = convTmpR[static_cast<size_t> (s)];
                float yR = lfShelfB[0] * xR + lfShelfStateR[src][0];
                lfShelfStateR[src][0] = lfShelfB[1] * xR - lfShelfA[1] * yR + lfShelfStateR[src][1];
                lfShelfStateR[src][1] = lfShelfB[2] * xR - lfShelfA[2] * yR;
                convTmpR[static_cast<size_t> (s)] = yR;
            }
        }

        // Apply ITD as fractional-sample delay if using aligned HRIRs.
        if (itdActive)
        {
            float itdL0 = currentITDL[src];
            float itdR0 = currentITDR[src];
            float itdL1 = targetITDL[src];
            float itdR1 = targetITDR[src];
            float invN = (numSamples > 1) ? 1.0f / static_cast<float> (numSamples - 1) : 1.0f;

            int wp = itdWritePos[src];

            for (int s = 0; s < numSamples; ++s)
            {
                float frac = static_cast<float> (s) * invN;
                float delL = itdL0 + frac * (itdL1 - itdL0);
                float delR = itdR0 + frac * (itdR1 - itdR0);

                // Write to circular ITD delay buffer
                itdBufferL[src][wp] = convTmpL[static_cast<size_t> (s)];
                itdBufferR[src][wp] = convTmpR[static_cast<size_t> (s)];

                // Read with fractional delay (linear interpolation)
                int idxL = static_cast<int> (delL);
                float fracL = delL - static_cast<float> (idxL);
                int rp0L = (wp - idxL + kITDBufferSize) & (kITDBufferSize - 1);
                int rp1L = (rp0L - 1 + kITDBufferSize) & (kITDBufferSize - 1);
                float sampleL = itdBufferL[src][rp0L] * (1.0f - fracL) + itdBufferL[src][rp1L] * fracL;

                int idxR = static_cast<int> (delR);
                float fracR = delR - static_cast<float> (idxR);
                int rp0R = (wp - idxR + kITDBufferSize) & (kITDBufferSize - 1);
                int rp1R = (rp0R - 1 + kITDBufferSize) & (kITDBufferSize - 1);
                float sampleR = itdBufferR[src][rp0R] * (1.0f - fracR) + itdBufferR[src][rp1R] * fracR;

                outL[s] += sampleL;
                outR[s] += sampleR;

                wp = (wp + 1) & (kITDBufferSize - 1);
            }

            itdWritePos[src] = wp;
            currentITDL[src] = itdL1;
            currentITDR[src] = itdR1;
        }
        else
        {
            // No ITD processing -- direct sum
            for (int s = 0; s < numSamples; ++s)
            {
                outL[s] += convTmpL[static_cast<size_t> (s)];
                outR[s] += convTmpR[static_cast<size_t> (s)];
            }
        }
    }
}

void BinauralRenderer::reset()
{
    for (int i = 0; i < MAX_SOURCES; ++i)
    {
        sourceConvL[i].reset();
        sourceConvR[i].reset();
        sourceConvReady[i] = false;
        currentITDL[i] = 0.0f;
        currentITDR[i] = 0.0f;
        targetITDL[i] = 0.0f;
        targetITDR[i] = 0.0f;
        itdWritePos[i] = 0;
        std::memset (itdBufferL[i], 0, sizeof (itdBufferL[i]));
        std::memset (itdBufferR[i], 0, sizeof (itdBufferR[i]));
        lfShelfStateL[i][0] = lfShelfStateL[i][1] = 0.0f;
        lfShelfStateR[i][0] = lfShelfStateR[i][1] = 0.0f;
    }
}

void BinauralRenderer::invalidateSources()
{
    for (int i = 0; i < MAX_SOURCES; ++i)
    {
        sourceConvL[i].clearAll();
        sourceConvR[i].clearAll();
        sourceConvReady[i] = false;
        cachedSourceAz[i] = -999.0f;
        cachedSourceEl[i] = -999.0f;
        currentITDL[i] = 0.0f;
        currentITDR[i] = 0.0f;
        targetITDL[i] = 0.0f;
        targetITDR[i] = 0.0f;
        itdWritePos[i] = 0;
        std::memset (itdBufferL[i], 0, sizeof (itdBufferL[i]));
        std::memset (itdBufferR[i], 0, sizeof (itdBufferR[i]));
        lfShelfStateL[i][0] = lfShelfStateL[i][1] = 0.0f;
        lfShelfStateR[i][0] = lfShelfStateR[i][1] = 0.0f;
    }
}

} // namespace spatialcore
