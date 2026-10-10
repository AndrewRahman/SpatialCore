#pragma once

#include <juce_dsp/juce_dsp.h>
#include <SpatialCore/Binaural/SharedFFTCache.h>
#include <SpatialCore/Binaural/TransitionTiming.h>
#include <vector>
#include <memory>

namespace spatialcore
{

//==============================================================================
// Partitioned Convolver -- real-time FFT overlap-save convolution
// Uses juce::dsp::FFT for efficient per-block convolution
//==============================================================================
class PartitionedConvolver
{
public:
    PartitionedConvolver() = default;

    /** Prepare the convolver for a given max block size and IR length.
        sampleRate only converts the ms-defined transition durations
        (TransitionTiming.h) to sample counts; invalid values (<= 0, > 1e6)
        fall back to 48 kHz timing.  Defaulted so existing 2-argument callers
        compile unchanged. */
    void prepare (int maxBlockSize, int irLength, double sampleRate = 48000.0);

    /** Set or update the impulse response.
        v1.0.5: Dual-convolver crossfade (issue #50) -- the new IR is loaded
        into the inactive slot, which warms for max(IR length,
        kHRIRWarmupMinMs) samples and then equal-power crossfades over
        kHRIRCrossfadeMs.  This eliminates overlap-save boundary
        discontinuities that caused audible pops during HRTF transitions.
        v2.0.0 (issue #234): both durations are counted in SAMPLES (converted
        from ms at the prepared sample rate), so the transition has the same
        wall-clock duration -- and is pop-free -- at every host buffer size and
        sample rate. */
    void setIR (const float* ir, int length);

    /** Process one block: convolve input with IR, write to output.
        in and out must be numSamples long. Can be called in-place. */
    void process (const float* in, float* out, int numSamples);

    /** Reset internal state (overlap buffers, input accumulators). */
    void reset();

    /** Reset and clear all IR data so the next setIR is a direct load (no crossfade). */
    void clearAll();

    bool isPrepared() const { return fftSize > 0; }

private:
    std::shared_ptr<juce::dsp::FFT> fft;  // Shared via process-global FFT cache (issue #131)
    int fftOrder  = 1;
    int fftSize   = 0;                 // 2^fftOrder
    int irLen     = 0;
    int blockSize = 0;

    // v1.0.5: Dual-convolver architecture (issue #50).
    // Two independent convolution slots run in parallel during crossfades.
    // This avoids the overlap-save boundary discontinuity: each slot keeps
    // its own overlap buffer tied to its own IR, so the tail is never
    // contaminated by a mismatched kernel.
    //
    // v2.0.0-dev.1 (issue #234): processSlot no longer requires numSamples to
    // equal the prepared `blockSize`. Each call's `numSamples` is processed as
    // its own independent overlap-add block (zero-padded to fftSize, which is
    // always >= numSamples + irLen - 1 since numSamples <= blockSize by
    // contract). `overlapAccum` is a persistent, pre-allocated (fftSize-long)
    // running accumulator: position 0 always holds the next not-yet-delivered
    // output sample. Every call ADDS this call's own linear-convolution result
    // into the accumulator, reads off the first `numSamples` as final output
    // (no future block can ever contribute to already-elapsed positions), then
    // shifts the accumulator left by `numSamples`. This is a direct
    // generalization of the fixed-block overlap-add scheme to variable
    // `numSamples`, provably identical to it when numSamples == blockSize
    // (verified: reduces to the same output/overlap update), and glitch-free /
    // latency-neutral for any numSamples in [1, blockSize] because it never
    // waits across calls to accumulate a full block before producing output.
    struct ConvSlot
    {
        std::vector<float> irFreqDomain;     // IR in frequency domain
        std::vector<float> fftWorkBuf;       // FFT work buffer (this call's own block)
        std::vector<float> overlapAccum;     // Persistent overlap-add accumulator (shifted each call)
    };

    ConvSlot slots[2];

    // State machine for IR transitions
    enum class State { Idle, Warmup, Crossfading };
    State state = State::Idle;

    // Issue #234: transition timing is counted in samples, derived from the
    // ms-defined constants in TransitionTiming.h at prepare() time.
    int activeSlot = 0;                          // Index of the currently active slot (0 or 1)
    int crossfadeSamples = 1024;                 // Equal-power crossfade length (21.333 ms)
    int warmupFloorSamples = 256;                // Warmup floor (5.333 ms), independent of host block size
    int warmupSamples = 256;                     // Warmup length of the current transition: max(irLen, floor)
    int stateSampleCount = 0;                    // Samples elapsed in the current Warmup / Crossfading state
    std::vector<float> fadeCurve;                // crossfadeSamples + 1 entries: sin (pi/2 * k / C); [0] = 0, [C] = 1

    // Deferred IR: if setIR() is called mid-transition, store it for later
    std::vector<float> pendingIR;
    int pendingIRLen = 0;
    bool hasPendingIR = false;

    // Work buffers for dual-slot output mixing
    std::vector<float> slotOutputA;
    std::vector<float> slotOutputB;

    // Helpers
    void processSlot (ConvSlot& slot, const float* in, float* out, int numSamples);
    void resetSlot (ConvSlot& slot);
    void loadIRIntoSlot (ConvSlot& slot, const float* ir, int length);
};

} // namespace spatialcore
