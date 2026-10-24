#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/TransitionTiming.h>
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

namespace spatialcore
{

//==============================================================================
// Binaural Renderer -- manages HRTF convolution for all algorithms
// Routes spatial accumulation buffers through per-source convolvers
//
// Thread contract (RTSF-03, D-09): one renderer is driven by one thread at a time.
// prepare() and setProfile() allocate and run off the audio thread (the message thread, or
// RenderEngine's loader thread on a renderer the audio thread is not using).
// updateSourceHRIR() and renderSourceBuffers() run on the audio thread. A setProfile() on a
// renderer the audio thread is using still runs, as it always did, but its database counts it
// (HRTFDatabase::getMutationWhileInUseCount()) and asserts in Debug. The safe route is
// RenderEngine::setHRTFProfile(), which hands a renderer over only while the audio thread is
// not using it.
//==============================================================================
class BinauralRenderer
{
public:
    BinauralRenderer() = default;

    /** Per-renderer HRTF database (issue Spatial-Media-Lab/OpenSpatialDelay#96: eliminates shared-state race
        between timer thread loading and audio thread HRIR lookups). */
    HRTFDatabase hrtfDatabase;

    /** Prepare all convolvers for the given sample rate and block size. Sizes the
        convolution scratch buffers for max (maxBlockSize, 512, loaded IR length).
        Allocates: never call from the audio thread. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Load a new HRTF profile. Computes normGain, prepares source convolvers and sizes the
        convolution scratch buffers for the loaded IR (so updateSourceHRIR never allocates).
        Allocates: never call from the audio thread. */
    void setProfile (int profileIndex);

    /** Update a single source's HRIR based on its current 3D position.
        Realtime-safe: KD-tree lookup + in-place FFT, no allocation.
        Called from processBlock at block boundaries when position changes.
        If the scratch buffers are smaller than the loaded IR (a prepare-contract violation,
        see getScratchTooSmallCount()) the call returns without touching anything: the source
        keeps its previous HRIR and the next call retries. It never grows a buffer. */
    void updateSourceHRIR (int sourceIndex, float azRad, float elRad);

    /** Render per-source accumulation buffers through HRTF convolvers.
        sourceBufs: [numSources][numSamples], outL/outR: [numSamples]
        Only enabled sources are convolved. outL/outR are zeroed first. If numSamples is larger
        than the scratch buffers (max (the block size prepare() was given, 512, the loaded IR
        length); see getScratchTooSmallCount()) the call outputs silence for that block (it leaves the zeroed
        output, so there is no "last good sound" to hold) and advances no convolver,
        ITD line or shelf state, so the renderer's next normal block is unaffected; it never
        grows a buffer. A caller with a longer block should render it in pieces. A zero or
        negative numSamples returns at once. */
    void renderSourceBuffers (const float* const* sourceBufs,
                              const bool* sourceEnabled,
                              int numSources, int numSamples,
                              float* outL, float* outR);

    /** How many updateSourceHRIR or renderSourceBuffers calls found the scratch buffers smaller
        than they needed. That only happens when the caller renders more samples than the scratch
        holds (max (the block size prepare() was given, 512, the loaded IR length); a block
        between the prepared size and that threshold is accepted and the convolver chunks it
        itself), or after shrinkScratchForTesting(). Each such call skipped its work instead of
        growing a buffer on the audio thread: updateSourceHRIR keeps the source's previous HRIR,
        and renderSourceBuffers leaves its zeroed output and advances no convolver, ITD line or
        shelf state. Debug builds also assert. Any thread, lock-free (D-03, D-04, RTSF-01). */
    int getScratchTooSmallCount() const noexcept;

    /** TEST-ONLY. Not part of the consumer API: a plugin must never call it. Shrinking the
        scratch below the loaded IR or the block size makes the audio path skip every call
        (silence, see getScratchTooSmallCount()) until the next prepare() or setProfile().
        Replaces both convolution scratch buffers with ones of exactly newSize floats
        (capacity too), so a test can reach the
        scratch-too-small guards, which prepare() and setProfile() otherwise make unreachable
        through the public API. Message or test thread only (it allocates). Declared
        unconditionally, like RenderEngine::setLoaderFailureForTesting(), so the class layout
        never depends on build settings. */
    void shrinkScratchForTesting (size_t newSize);

    /** Check if the renderer is in Simple (Woodworth) mode. */
    bool isSimpleMode() const { return activeProfile == 0; }

    /** Get the active profile index. */
    int  getActiveProfile() const { return activeProfile; }

    /** Reset all convolver states (e.g., on playback restart). */
    void reset();

    /** Invalidate all source convolvers so the next HRIR load is a direct load
        (no crossfade from stale IR). Used during preset transitions to prevent
        clicks from crossfading between unrelated HRIR positions. */
    void invalidateSources();

    /** Test-only: override ITD processing state for diagnostic isolation. */
    void setITDEnabled (bool enabled) { itdActive = enabled; }

    /** Test-only: read current ITD values for diagnostic logging. */
    float getCurrentITDL (int src) const { return (src >= 0 && src < MAX_SOURCES) ? currentITDL[src] : 0.0f; }
    float getCurrentITDR (int src) const { return (src >= 0 && src < MAX_SOURCES) ? currentITDR[src] : 0.0f; }
    float getTargetITDL (int src) const { return (src >= 0 && src < MAX_SOURCES) ? targetITDL[src] : 0.0f; }
    float getTargetITDR (int src) const { return (src >= 0 && src < MAX_SOURCES) ? targetITDR[src] : 0.0f; }

    /** Test-only: no-op, retained for API compatibility. */
    void setMinPhaseEnabled (bool) {}

private:
    // Per-source direct binaural convolvers
    PartitionedConvolver sourceConvL[MAX_SOURCES];
    PartitionedConvolver sourceConvR[MAX_SOURCES];
    float  cachedSourceAz[MAX_SOURCES] = {};     // radians, for ~1 degree change detection
    float  cachedSourceEl[MAX_SOURCES] = {};
    bool   sourceConvReady[MAX_SOURCES] = {};     // true after first HRIR loaded
    float  storedNormGain  = 1.0f;                // cross-profile normalization
    int    storedIRLength  = 0;                    // cached for updateSourceHRIR

    int    activeProfile    = 0;   // Default = Simple (Woodworth fallback)
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    // Temporary work buffers for convolution output
    std::vector<float> convTmpL, convTmpR;

    // Counted by the scratch-too-small guards (see getScratchTooSmallCount()). Relaxed and
    // lock-free, so any thread may read it.
    std::atomic<int> scratchTooSmallCount_ { 0 };

    /** The one guard behind both audio-thread entry points (D-03, D-04, RTSF-01). True when both
        scratch buffers hold at least `needed` floats. Otherwise it counts the miss in
        scratchTooSmallCount_, asserts (Debug only) and returns false, and the caller skips its
        work. It never resizes, in any build type. */
    bool scratchFits (size_t needed) noexcept;

    /** Grows convTmpL/convTmpR to max (currentBlockSize, 512, storedIRLength). Called from
        prepare() and setProfile() only (message or loader thread). Grow-only. */
    void ensureScratchCapacity();

    // ITD (Inter-aural Time Difference) tracking for smooth HRIR transitions.
    // When using getAlignedHRIR(), the two ears' HRIRs are time-aligned to the
    // EARLIER ear's onset (the interaural shift is removed; the common leading
    // onset stays in both waveforms).  The interaural shift is applied here as a
    // separate per-ear fractional-sample delay (0 for the leading ear), so the
    // rendered per-ear onset equals the raw HRIR onset (WR-01, O212).
    //
    // The applied ITD is a MIRROR of the per-source PartitionedConvolver
    // transition (Spatial-Media-Lab/OpenSpatialDelay#234, D-06): same warmup max(irLen, kHRIRWarmupMinMs), same
    // kHRIRCrossfadeMs window, same latest-wins coalescing, counted on the same
    // samples, so the time offset and the HRIR tone change move together as ONE
    // transition.  A first load snaps (nothing audible to slew from).  If the
    // convolver's state machine ever changes, this mirror must change with it
    // ([itd][slew][align] fails otherwise).
    float currentITDL[MAX_SOURCES] = {};    // Current applied ITD (samples, fractional)
    float currentITDR[MAX_SOURCES] = {};
    float targetITDL[MAX_SOURCES]  = {};    // Target ITD from latest HRIR lookup
    float targetITDR[MAX_SOURCES]  = {};

    enum class ITDPhase { Idle = 0, Warmup, Ramp };
    ITDPhase itdPhase[MAX_SOURCES] = {};             // mirrors the convolver Idle / Warmup / Crossfading
    int   itdPhaseRemaining[MAX_SOURCES] = {};       // samples left in the current Warmup or Ramp
    float itdStepL[MAX_SOURCES] = {};                // per-sample ramp increment
    float itdStepR[MAX_SOURCES] = {};
    float itdRampTargetL[MAX_SOURCES] = {};          // ITD the in-flight transition moves to (targetITD = latest lookup)
    float itdRampTargetR[MAX_SOURCES] = {};
    float itdPendingL[MAX_SOURCES] = {};             // latest retarget that arrived mid-transition
    float itdPendingR[MAX_SOURCES] = {};
    bool  itdHasPending[MAX_SOURCES] = {};
    bool  sourceConvHasIR[MAX_SOURCES] = {};         // convolvers hold an IR: next setIR is a crossfade, not a direct load
    int   itdXfadeSamples = 1024;                    // kHRIRCrossfadeMs at currentSampleRate (set in setProfile)
    int   itdWarmupFloorSamples = 256;               // kHRIRWarmupMinMs at currentSampleRate (set in setProfile)

    // Short delay lines for ITD application.  All five bundled SOFA files have
    // Data.Delay = 0, so getAlignedHRIR's onset fallback supplies the delay: the
    // per-ear onset SHIFT relative to the earlier ear (WR-01, O212), i.e. 0 for
    // the leading ear and the ITD (up to about +-48 samples at 48 kHz, scaling
    // with the sample rate) for the other.  Before O212 the absolute onset was
    // carried (SADIE up to about 124 at 48 kHz), which is why the ring grew from
    // 64 to 512 in O211 (D-13); 512 now stays as headroom for high sample rates
    // and custom SOFA files with real Data.Delay values.  Larger delays
    // (untrusted) are clamped to kMaxITDSamples.  This resolves the wrap that
    // Phase 3 recorded here as a deferred defect (AndrewRahman/SpatialCore#25,
    // WR-05, D-16): both halves of its intended fix (a larger ring, delays
    // relative to min (delayL, delayR)) are in, and BinauralRendererTests pins the
    // new timing.  Must stay a power of two (mask indexing).
    static constexpr int kITDBufferSize = 512;
    static constexpr int kMaxITDSamples = kITDBufferSize - 2;   // 2-tap read needs index + 1 inside the ring
    float itdBufferL[MAX_SOURCES][kITDBufferSize] = {};
    float itdBufferR[MAX_SOURCES][kITDBufferSize] = {};
    int   itdWritePos[MAX_SOURCES] = {};
    bool  itdActive = false;  // true when using aligned HRIRs (non-Simple profiles)

    // Low-shelf bass compensation for bass-deficient HRTFs.
    // MIT KEMAR has a 24 dB deficit at 50 Hz (measurement limitation). A low-shelf
    // filter boosts the convolver output below 200 Hz to compensate.
    bool lfShelfActive = false;
    // Per-source IIR state for 2nd-order low-shelf (biquad)
    float lfShelfStateL[MAX_SOURCES][2] = {};  // z^-1, z^-2 for left channel
    float lfShelfStateR[MAX_SOURCES][2] = {};  // z^-1, z^-2 for right channel
    float lfShelfB[3] = {};  // feedforward coefficients
    float lfShelfA[3] = {};  // feedback coefficients (a[0] = 1.0)
};

} // namespace spatialcore
