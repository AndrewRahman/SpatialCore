#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <cstring>

namespace spatialcore
{

// Process-global FFT cache (issue Spatial-Media-Lab/OpenSpatialDelay#131).  Apple's vDSP shares internal
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

    // v1.0.5: Allocate dual convolution slots (issue Spatial-Media-Lab/OpenSpatialDelay#50)
    for (int s = 0; s < 2; ++s)
    {
        slots[s].irFreqDomain.assign (static_cast<size_t> (fftSize * 2), 0.0f);
        slots[s].fftWorkBuf.resize (static_cast<size_t> (fftSize * 2), 0.0f);
        slots[s].overlapAccum.assign (static_cast<size_t> (fftSize), 0.0f);
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
    std::fill (slot.overlapAccum.begin(), slot.overlapAccum.end(), 0.0f);
    std::fill (slot.fftWorkBuf.begin(), slot.fftWorkBuf.end(), 0.0f);
}

void PartitionedConvolver::setIR (const float* ir, int length)
{
    if (fftSize == 0) return;

    irLen = length;

    // v1.0.5: Dual-convolver IR loading strategy (issue Spatial-Media-Lab/OpenSpatialDelay#50).
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
    loadIRIntoSlot (slots[static_cast<size_t> (inactiveSlot)], ir, length);

    state = State::Warmup;
    stateBlockCount = 0;
}

void PartitionedConvolver::processSlot (ConvSlot& slot, const float* in, float* out, int numSamples)
{
    // v2.0.0-dev.1 (issue Spatial-Media-Lab/OpenSpatialDelay#234): decoupled from the prepared `blockSize`.
    // Every call's `numSamples` (host block, NOT the prepared block) is its
    // own independent overlap-add block. This never waits across calls to
    // accumulate a full prepared-size block, so it stays glitch-free and
    // latency-neutral for numSamples < blockSize and for variable numSamples.
    jassert (numSamples > 0 && numSamples <= blockSize);
    jassert (numSamples + irLen - 1 <= fftSize);  // linear-conv always fits (numSamples <= blockSize by contract)

    // Zero-pad this call's new input into the FFT work buffer.
    std::fill (slot.fftWorkBuf.begin(), slot.fftWorkBuf.end(), 0.0f);
    for (int i = 0; i < numSamples; ++i)
        slot.fftWorkBuf[static_cast<size_t> (i)] = in[i];

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

    // Accumulate this block's own linear-conv result into the persistent
    // overlap-add accumulator (position 0 == next not-yet-delivered sample).
    for (int i = 0; i < fftSize; ++i)
        slot.overlapAccum[static_cast<size_t> (i)] += slot.fftWorkBuf[static_cast<size_t> (i)];

    // The first numSamples positions are now final: no future block's input
    // (which starts at or after the current position + numSamples) can ever
    // contribute to them.
    for (int i = 0; i < numSamples; ++i)
        out[i] = slot.overlapAccum[static_cast<size_t> (i)];

    // Shift the accumulator left by numSamples, discarding delivered samples
    // and zero-filling the newly exposed tail. Any still-pending contribution
    // from an earlier block that spilled further than this call's numSamples
    // (possible when numSamples shrinks between calls) is preserved by the
    // shift rather than dropped.
    std::memmove (slot.overlapAccum.data(), slot.overlapAccum.data() + numSamples,
                  sizeof (float) * static_cast<size_t> (fftSize - numSamples));
    std::fill (slot.overlapAccum.end() - numSamples, slot.overlapAccum.end(), 0.0f);
}

void PartitionedConvolver::process (const float* in, float* out, int numSamples)
{
    // WR-02: runtime guard against an oversized host block. The Spatial-Media-Lab/OpenSpatialDelay#234 decoupling
    // contract is numSamples <= blockSize (the prepared maxBlockSize); processSlot
    // relies on it — overlapAccum has length fftSize, and the tail memmove of size
    // (fftSize - numSamples) underflows to a huge size_t when numSamples > fftSize,
    // causing OOB read/write and a likely crash. The jassert there compiles out in
    // Release, so a host that ever delivers a larger block (some exceed
    // maximumExpectedSamplesPerBlock) is unprotected. Split any oversized block into
    // <= blockSize chunks: each chunk is an independent, valid overlap-add sub-block
    // (processSlot carries overlapAccum across calls), so the output stays correct.
    if (blockSize > 0 && numSamples > blockSize)
    {
        for (int offset = 0; offset < numSamples; )
        {
            const int chunk = std::min (blockSize, numSamples - offset);
            process (in + offset, out + offset, chunk);
            offset += chunk;
        }
        return;
    }

    if (fftSize == 0 || irLen == 0)
    {
        // Pass-through if no IR set
        if (in != out)
            std::memcpy (out, in, sizeof (float) * static_cast<size_t> (numSamples));
        return;
    }

    // v1.0.5: Dual-convolver state machine (issue Spatial-Media-Lab/OpenSpatialDelay#50).
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
