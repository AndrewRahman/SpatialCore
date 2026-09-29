#pragma once

#include <juce_dsp/juce_dsp.h>
#include <SpatialCore/Binaural/SharedFFTCache.h>
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

    /** Prepare the convolver for a given max block size and IR length. */
    void prepare (int maxBlockSize, int irLength);

    /** Set or update the impulse response.
        v1.0.5: Dual-convolver crossfade -- new IR is loaded into the inactive
        slot and crossfaded over kCrossfadeBlocks blocks using equal-power
        (cos/sin) gains.  This eliminates overlap-save boundary discontinuities
        that caused audible pops during HRTF transitions (issue Spatial-Media-Lab/OpenSpatialDelay#50). */
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
    std::shared_ptr<juce::dsp::FFT> fft;  // Shared via process-global FFT cache (issue Spatial-Media-Lab/OpenSpatialDelay#131)
    int fftOrder  = 1;
    int fftSize   = 0;                 // 2^fftOrder
    int irLen     = 0;
    int blockSize = 0;

    // v1.0.5: Dual-convolver architecture (issue Spatial-Media-Lab/OpenSpatialDelay#50).
    // Two independent convolution slots run in parallel during crossfades.
    // This avoids the overlap-save boundary discontinuity: each slot keeps
    // its own overlap buffer tied to its own IR, so the tail is never
    // contaminated by a mismatched kernel.
    //
    // v2.0.0-dev.1 (issue Spatial-Media-Lab/OpenSpatialDelay#234): processSlot no longer requires numSamples to
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

    static constexpr int kCrossfadeBlocks = 4;   // Equal-power crossfade duration (~21ms)
    static constexpr int kWarmupBlocks = 1;      // Let inactive slot build overlap before crossfade

    int activeSlot = 0;                          // Index of the currently active slot (0 or 1)
    int stateBlockCount = 0;                     // Blocks elapsed in current state

    // Per-sample gain interpolation for glitch-free crossfade
    float fadeOutGain = 1.0f;                    // Current fade-out gain (active -> old)
    float fadeInGain  = 0.0f;                    // Current fade-in gain  (inactive -> new)
    float prevFadeOutGain = 1.0f;                // Previous block's ending fade-out gain
    float prevFadeInGain  = 0.0f;               // Previous block's ending fade-in gain

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
