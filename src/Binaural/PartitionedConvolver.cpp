#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <cstring>

namespace spatialcore
{

// Process-global FFT cache (D-08; the full contract, provenance and platform limit are written in
// SharedFFTCache.h). One FFT per order, created on the first request under the SpinLock at prepare
// time (PartitionedConvolver::prepare, HRTFDatabase::convertToMinPhase), never from the audio
// thread, and kept for the life of the process: a deliberate permanent allocation, not a leak,
// which is why a Debug build prints "Leaked objects detected: 1 FFT" at exit. It came from
// Spatial-Media-Lab/OpenSpatialDelay#142 (PR) for Spatial-Media-Lab/OpenSpatialDelay#131. Individual convolvers also
// hold shared_ptrs, so an FFT outlives any one convolver.
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

void PartitionedConvolver::prepare (int maxBlockSize, int irLength_, double sampleRate)
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
        slots[s].irTime.assign (static_cast<size_t> (fftSize), 0.0f);   // Spatial-Media-Lab/OpenSpatialDelay#234 pulsing: rho source, never resized on the audio thread
        slots[s].irEnergy = 0.0;
    }
    crossfadeRho = 0.0f;

    // Work buffers for dual-slot output mixing
    slotOutputA.resize (static_cast<size_t> (maxBlockSize), 0.0f);
    slotOutputB.resize (static_cast<size_t> (maxBlockSize), 0.0f);

    // Pending IR buffer (deferred updates during transitions)
    pendingIR.resize (static_cast<size_t> (irLen), 0.0f);
    pendingIRLen = 0;
    hasPendingIR = false;

    // Issue Spatial-Media-Lab/OpenSpatialDelay#234: transition durations are ms-defined and counted in samples.
    // The crossfade gains are correlation-compensated per transition
    // (computeCrossfadeGains, Spatial-Media-Lab/OpenSpatialDelay#234 "pulsing"), computed per sample.
    crossfadeSamples   = hrirTransitionMsToSamples (sampleRate, kHRIRCrossfadeMs);
    warmupFloorSamples = hrirTransitionMsToSamples (sampleRate, kHRIRWarmupMinMs);
    warmupSamples      = std::max (irLen, warmupFloorSamples);

    reset();
}

void PartitionedConvolver::loadIRIntoSlot (ConvSlot& slot, const float* ir, int length)
{
    std::fill (slot.irFreqDomain.begin(), slot.irFreqDomain.end(), 0.0f);
    std::fill (slot.irTime.begin(), slot.irTime.end(), 0.0f);
    double energy = 0.0;
    const int n = std::min (length, std::min (fftSize, static_cast<int> (slot.irTime.size())));
    for (int i = 0; i < n; ++i)
    {
        slot.irFreqDomain[static_cast<size_t> (i)] = ir[i];
        slot.irTime[static_cast<size_t> (i)] = ir[i];
        energy += static_cast<double> (ir[i]) * static_cast<double> (ir[i]);
    }
    slot.irEnergy = energy;
    fft->performRealOnlyForwardTransform (slot.irFreqDomain.data(), true);
}

// Spatial-Media-Lab/OpenSpatialDelay#234 "pulsing": normalised zero-lag correlation of two slots' time-domain IRs,
// clamped to [0, 1] (anti-correlated pairs are treated as uncorrelated) and 0 for
// a zero-norm IR.  O(irLen), no allocation -- called once per transition start.
float PartitionedConvolver::computeIRCorrelation (const ConvSlot& a, const ConvSlot& b) noexcept
{
    if (a.irEnergy <= 0.0 || b.irEnergy <= 0.0)
        return 0.0f;
    const size_t n = std::min (a.irTime.size(), b.irTime.size());
    double dot = 0.0;
    for (size_t i = 0; i < n; ++i)
        dot += static_cast<double> (a.irTime[i]) * static_cast<double> (b.irTime[i]);
    const double rho = dot / std::sqrt (a.irEnergy * b.irEnergy);
    if (! (rho > 0.0)) return 0.0f;   // also catches NaN
    return static_cast<float> (std::min (rho, 1.0));
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

    // Spatial-Media-Lab/OpenSpatialDelay#234 "pulsing": measure how coherent the outgoing and incoming IRs are, so
    // the crossfade keeps constant power for this pair.  This is also the path a
    // coalesced pendingIR takes when it becomes the new target (process() calls
    // setIR after the active slot swap), so rho is always vs the IR being left.
    crossfadeRho = computeIRCorrelation (slots[static_cast<size_t> (activeSlot)],
                                         slots[static_cast<size_t> (inactiveSlot)]);

    // Issue Spatial-Media-Lab/OpenSpatialDelay#234: warm for max(IR length, floor) samples -- anchored to the IR
    // length (the slot needs a full IR of history), never to the host block size.
    state = State::Warmup;
    warmupSamples = std::max (length, warmupFloorSamples);
    stateSampleCount = 0;
}

void PartitionedConvolver::processSlot (ConvSlot& slot, const float* in, float* out, int numSamples)
{
    // v2.0.0-dev.1 (issue Spatial-Media-Lab/OpenSpatialDelay#234): decoupled from the prepared `blockSize`.
    // Every call's `numSamples` (host block, NOT the prepared block) is its
    // own independent overlap-add block. This never waits across calls to
    // accumulate a full prepared-size block, so it stays glitch-free and
    // latency-neutral for numSamples < blockSize and for variable numSamples.
    //
    // This per-call overlap-add is ONE HALF of the Spatial-Media-Lab/OpenSpatialDelay#234 small-block fix. The other half
    // is in process(): the IR warm-up and crossfade are counted in samples from
    // TransitionTiming.h, never in calls, so a moving source no longer steps harder at 32-128
    // samples than at 512. Both halves are covered by [convolver][blocksize] (this function
    // against a double-precision direct convolution), [convolver][transition], and
    // [bug02][moving] and [bug02][steady] (through the engine).
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
    // Issue Spatial-Media-Lab/OpenSpatialDelay#234: an empty host call never touches transition state.
    if (numSamples <= 0)
        return;

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

    // v1.0.5: Dual-convolver state machine (issue Spatial-Media-Lab/OpenSpatialDelay#50), sample-counted (Spatial-Media-Lab/OpenSpatialDelay#234).
    // Three states: Idle (single slot), Warmup (both slots process, output only
    // the active one), Crossfading (exact per-sample correlation-compensated
    // blend, computeCrossfadeGains with this transition's rho).  Warmup lasts warmupSamples = max(irLen, floor) and the
    // crossfade lasts crossfadeSamples, both counted in samples, so a state
    // change may begin or end mid-block and the transition has the same
    // wall-clock duration at every host buffer size.
    switch (state)
    {
        case State::Idle:
        {
            // Only the active slot processes
            processSlot (slots[static_cast<size_t> (activeSlot)], in, out, numSamples);
            break;
        }

        case State::Warmup:
        case State::Crossfading:
        {
            // Both slots render into private buffers before mixing, so `out`
            // may alias `in` without the second slot reading clobbered input.
            processSlot (slots[static_cast<size_t> (activeSlot)],     in, slotOutputA.data(), numSamples);
            processSlot (slots[static_cast<size_t> (1 - activeSlot)], in, slotOutputB.data(), numSamples);

            int i = 0;

            if (state == State::Warmup)
            {
                // Output only the active slot while the inactive one builds history.
                const int n = std::min (numSamples, warmupSamples - stateSampleCount);
                std::memcpy (out, slotOutputA.data(), sizeof (float) * static_cast<size_t> (n));
                i += n;
                stateSampleCount += n;

                if (stateSampleCount >= warmupSamples)
                {
                    state = State::Crossfading;
                    stateSampleCount = 0;   // crossfade may start mid-block
                }
            }

            if (state == State::Crossfading && i < numSamples)
            {
                const int n = std::min (numSamples - i, crossfadeSamples - stateSampleCount);
                const float* a = slotOutputA.data();
                const float* b = slotOutputB.data();

                for (int j = 0; j < n; ++j)
                {
                    const int k = stateSampleCount + j + 1;   // 1..crossfadeSamples
                    float gOut, gIn;
                    computeCrossfadeGains (k, crossfadeSamples, crossfadeRho, gOut, gIn);
                    out[i + j] = a[i + j] * gOut + b[i + j] * gIn;
                }

                i += n;
                stateSampleCount += n;

                if (stateSampleCount >= crossfadeSamples)
                {
                    // Crossfade finished (possibly mid-block): the rest of this
                    // block is the new IR at unity gain.
                    if (i < numSamples)
                        std::memcpy (out + i, slotOutputB.data() + i,
                                     sizeof (float) * static_cast<size_t> (numSamples - i));

                    // Swap active slot to the new one
                    activeSlot = 1 - activeSlot;
                    state = State::Idle;
                    stateSampleCount = 0;

                    // Apply any pending IR that arrived during the transition
                    if (hasPendingIR)
                    {
                        hasPendingIR = false;
                        setIR (pendingIR.data(), pendingIRLen);
                    }
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
    stateSampleCount = 0;
    hasPendingIR = false;
    pendingIRLen = 0;
}

void PartitionedConvolver::clearAll()
{
    reset();
    // Zero frequency-domain IR data so the next setIR does a direct load
    // instead of crossfading from stale IR (used during preset transitions)
    for (int s = 0; s < 2; ++s)
    {
        std::fill (slots[s].irFreqDomain.begin(), slots[s].irFreqDomain.end(), 0.0f);
        std::fill (slots[s].irTime.begin(), slots[s].irTime.end(), 0.0f);
        slots[s].irEnergy = 0.0;
    }
}

} // namespace spatialcore
