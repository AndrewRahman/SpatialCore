#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <cstring>

namespace spatialcore
{

// Process-global FFT cache (issue #131).  Apple's vDSP shares internal
// twiddle factor memory across FFT setups of the same order.  Destroying the
// last setup of a given order frees the shared table even while another
// thread's vDSP_fft_zrip is reading from it.  The cache creates each order
// once and holds a permanent shared_ptr so the vDSP twiddle tables are never
// freed while the process is alive.  Individual convolvers also hold
// shared_ptrs, providing redundant safety.
std::shared_ptr<juce::dsp::FFT> SharedFFTCache::getOrCreate (int fftOrder)
{
    juce::SpinLock::ScopedLockType sl (lock);
    auto it = cache.find (fftOrder);
    if (it != cache.end())
        return it->second;
    auto ptr = std::make_shared<juce::dsp::FFT> (fftOrder);
    cache[fftOrder] = ptr;
    return ptr;
}

SharedFFTCache& getSharedFFTCache()
{
    static SharedFFTCache instance;
    return instance;
}

void PartitionedConvolver::prepare (int maxBlockSize, int irLength_)
{
    irLen = irLength_;
    blockSize = maxBlockSize;

    // FFT size must be >= blockSize + irLen - 1 (linear convolution length)
    // Round up to next power of 2
    int minFFTSize = blockSize + irLen - 1;
    int newFftOrder = 1;
    while ((1 << newFftOrder) < minFFTSize)
        ++newFftOrder;

    bool orderChanged = (newFftOrder != fftOrder) || fft == nullptr;
    fftOrder = newFftOrder;
    fftSize = 1 << fftOrder;

    if (orderChanged)
        fft = getSharedFFTCache().getOrCreate (fftOrder);

    // v1.0.5: Allocate dual convolution slots (issue #50)
    for (int s = 0; s < 2; ++s)
    {
        slots[s].irFreqDomain.assign (static_cast<size_t> (fftSize * 2), 0.0f);
        slots[s].inputAccum.resize (static_cast<size_t> (fftSize * 2), 0.0f);
        slots[s].fftWorkBuf.resize (static_cast<size_t> (fftSize * 2), 0.0f);
        slots[s].overlapBuf.resize (static_cast<size_t> (fftSize), 0.0f);
        slots[s].inputAccumPos = 0;
    }

    // Work buffers for dual-slot output mixing
    slotOutputA.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    slotOutputB.resize (static_cast<size_t> (maxBlockSize), 0.0f);

    // Pending IR buffer (deferred updates during transitions)
    pendingIR.resize (static_cast<size_t> (irLen), 0.0f);
    pendingIRLen = 0;
    hasPendingIR = false;

    reset();
}

void PartitionedConvolver::loadIRIntoSlot (ConvSlot& slot, const float* ir, int length)
{
    std::fill (slot.irFreqDomain.begin(), slot.irFreqDomain.end(), 0.0f);
    for (int i = 0; i < std::min (length, fftSize); ++i)
        slot.irFreqDomain[static_cast<size_t> (i)] = ir[i];
    fft->performRealOnlyForwardTransform (slot.irFreqDomain.data(), true);
}

void PartitionedConvolver::resetSlot (ConvSlot& slot)
{
    std::fill (slot.inputAccum.begin(), slot.inputAccum.end(), 0.0f);
    std::fill (slot.overlapBuf.begin(), slot.overlapBuf.end(), 0.0f);
    std::fill (slot.fftWorkBuf.begin(), slot.fftWorkBuf.end(), 0.0f);
    slot.inputAccumPos = 0;
}

void PartitionedConvolver::setIR (const float* ir, int length)
{
    if (fftSize == 0) return;

    irLen = length;

    // v1.0.5: Dual-convolver IR loading strategy (issue #50).
    // First IR ever: load directly into the active slot, no crossfade.
    if (slots[static_cast<size_t> (activeSlot)].irFreqDomain.empty() ||
        std::all_of (slots[static_cast<size_t> (activeSlot)].irFreqDomain.begin(),
                     slots[static_cast<size_t> (activeSlot)].irFreqDomain.end(),
                     [] (float v) { return v == 0.0f; }))
    {
        loadIRIntoSlot (slots[static_cast<size_t> (activeSlot)], ir, length);
        return;
    }

    // Mid-transition (Warmup or Crossfading): defer to pendingIR
    if (state != State::Idle)
    {
        for (int i = 0; i < std::min (length, static_cast<int> (pendingIR.size())); ++i)
            pendingIR[static_cast<size_t> (i)] = ir[i];
        pendingIRLen = length;
        hasPendingIR = true;
        return;
    }

    // Idle: load new IR into inactive slot, enter Warmup
    int inactiveSlot = 1 - activeSlot;
    resetSlot (slots[static_cast<size_t> (inactiveSlot)]);
    // Sync input accumulator position so both slots process in lockstep
    slots[static_cast<size_t> (inactiveSlot)].inputAccumPos =
        slots[static_cast<size_t> (activeSlot)].inputAccumPos;
    loadIRIntoSlot (slots[static_cast<size_t> (inactiveSlot)], ir, length);

    state = State::Warmup;
    stateBlockCount = 0;
}

void PartitionedConvolver::processSlot (ConvSlot& slot, const float* in, float* out, int numSamples)
{
    // Standard overlap-save convolution on a single ConvSlot
    int samplesProcessed = 0;

    while (samplesProcessed < numSamples)
    {
        int spaceInAccum = blockSize - slot.inputAccumPos;
        int samplesToAccum = std::min (spaceInAccum, numSamples - samplesProcessed);

        for (int i = 0; i < samplesToAccum; ++i)
            slot.inputAccum[static_cast<size_t> (slot.inputAccumPos + i)] = in[samplesProcessed + i];

        slot.inputAccumPos += samplesToAccum;
        samplesProcessed += samplesToAccum;

        if (slot.inputAccumPos >= blockSize)
        {
            // Copy input to work buffer, zero-pad to fftSize
            std::fill (slot.fftWorkBuf.begin(), slot.fftWorkBuf.end(), 0.0f);
            for (int i = 0; i < blockSize; ++i)
                slot.fftWorkBuf[static_cast<size_t> (i)] = slot.inputAccum[static_cast<size_t> (i)];

            // Forward FFT of input
            fft->performRealOnlyForwardTransform (slot.fftWorkBuf.data(), true);

            // Complex multiply with slot's IR spectrum
            for (int i = 0; i < fftSize * 2; i += 2)
            {
                float re1 = slot.fftWorkBuf[static_cast<size_t> (i)],     im1 = slot.fftWorkBuf[static_cast<size_t> (i + 1)];
                float re2 = slot.irFreqDomain[static_cast<size_t> (i)],   im2 = slot.irFreqDomain[static_cast<size_t> (i + 1)];
                slot.fftWorkBuf[static_cast<size_t> (i)]     = re1 * re2 - im1 * im2;
                slot.fftWorkBuf[static_cast<size_t> (i + 1)] = re1 * im2 + im1 * re2;
            }

            fft->performRealOnlyInverseTransform (slot.fftWorkBuf.data());

            int outStart = samplesProcessed - blockSize;
            int outSamples = std::min (blockSize, numSamples - outStart);

            for (int i = 0; i < outSamples; ++i)
            {
                out[outStart + i] = slot.fftWorkBuf[static_cast<size_t> (i)]
                                  + slot.overlapBuf[static_cast<size_t> (i)];
            }

            // Save overlap for next block
            int overlapLen = fftSize - blockSize;
            for (int i = 0; i < overlapLen; ++i)
                slot.overlapBuf[static_cast<size_t> (i)] = slot.fftWorkBuf[static_cast<size_t> (blockSize + i)];
            for (int i = overlapLen; i < fftSize; ++i)
                slot.overlapBuf[static_cast<size_t> (i)] = 0.0f;

            slot.inputAccumPos = 0;
        }
    }
}

void PartitionedConvolver::process (const float* in, float* out, int numSamples)
{
    if (fftSize == 0 || irLen == 0)
    {
        // Pass-through if no IR set
        if (in != out)
            std::memcpy (out, in, sizeof (float) * static_cast<size_t> (numSamples));
        return;
    }

    // v1.0.5: Dual-convolver state machine (issue #50).
    // Three states: Idle (single slot), Warmup (both process, output only active),
    // Crossfading (equal-power cos/sin blend with per-sample gain interpolation).
    switch (state)
    {
        case State::Idle:
        {
            // Only the active slot processes
            processSlot (slots[static_cast<size_t> (activeSlot)], in, out, numSamples);
            break;
        }

        case State::Warmup:
        {
            // Both slots process, but output only from active slot.
            // This lets the inactive slot build up its overlap buffer.
            processSlot (slots[static_cast<size_t> (activeSlot)], in, out, numSamples);
            processSlot (slots[static_cast<size_t> (1 - activeSlot)], in, slotOutputB.data(), numSamples);

            ++stateBlockCount;
            if (stateBlockCount >= kWarmupBlocks)
            {
                state = State::Crossfading;
                stateBlockCount = 0;
                // Initialize crossfade gains
                prevFadeOutGain = 1.0f;
                prevFadeInGain  = 0.0f;
            }
            break;
        }

        case State::Crossfading:
        {
            // Both slots process into separate buffers
            processSlot (slots[static_cast<size_t> (activeSlot)], in, slotOutputA.data(), numSamples);
            processSlot (slots[static_cast<size_t> (1 - activeSlot)], in, slotOutputB.data(), numSamples);

            ++stateBlockCount;

            // Compute equal-power crossfade gains for this block's END
            float progress = static_cast<float> (stateBlockCount) / static_cast<float> (kCrossfadeBlocks);
            if (progress > 1.0f) progress = 1.0f;

            constexpr float halfPi = juce::MathConstants<float>::halfPi;
            fadeOutGain = std::cos (progress * halfPi);   // 1 -> 0
            fadeInGain  = std::sin (progress * halfPi);   // 0 -> 1

            // Per-sample linear interpolation between previous and current gains
            float fadeOutInc = (fadeOutGain - prevFadeOutGain) / static_cast<float> (numSamples);
            float fadeInInc  = (fadeInGain  - prevFadeInGain)  / static_cast<float> (numSamples);

            float gOut = prevFadeOutGain;
            float gIn  = prevFadeInGain;

            for (int i = 0; i < numSamples; ++i)
            {
                gOut += fadeOutInc;
                gIn  += fadeInInc;
                out[i] = slotOutputA[static_cast<size_t> (i)] * gOut
                       + slotOutputB[static_cast<size_t> (i)] * gIn;
            }

            prevFadeOutGain = fadeOutGain;
            prevFadeInGain  = fadeInGain;

            // Check if crossfade is complete
            if (stateBlockCount >= kCrossfadeBlocks)
            {
                // Swap active slot to the new one
                activeSlot = 1 - activeSlot;
                state = State::Idle;
                stateBlockCount = 0;
                fadeOutGain = 1.0f;
                fadeInGain  = 0.0f;
                prevFadeOutGain = 1.0f;
                prevFadeInGain  = 0.0f;

                // Apply any pending IR that arrived during the transition
                if (hasPendingIR)
                {
                    hasPendingIR = false;
                    setIR (pendingIR.data(), pendingIRLen);
                }
            }
            break;
        }
    }
}

void PartitionedConvolver::reset()
{
    for (int s = 0; s < 2; ++s)
        resetSlot (slots[s]);

    state = State::Idle;
    activeSlot = 0;
    stateBlockCount = 0;
    fadeOutGain = 1.0f;
    fadeInGain  = 0.0f;
    prevFadeOutGain = 1.0f;
    prevFadeInGain  = 0.0f;
    hasPendingIR = false;
    pendingIRLen = 0;
}

void PartitionedConvolver::clearAll()
{
    reset();
    // Zero frequency-domain IR data so the next setIR does a direct load
    // instead of crossfading from stale IR (used during preset transitions)
    for (int s = 0; s < 2; ++s)
        std::fill (slots[s].irFreqDomain.begin(), slots[s].irFreqDomain.end(), 0.0f);
}

} // namespace spatialcore
