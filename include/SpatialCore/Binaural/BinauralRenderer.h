#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
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
    // When using getAlignedHRIR(), HRIRs are time-aligned (ITD removed).
    // ITD is applied as a separate fractional-sample delay, smoothly interpolated
    // between blocks to prevent timing discontinuities.
    float currentITDL[MAX_SOURCES] = {};    // Current applied ITD (samples, fractional)
    float currentITDR[MAX_SOURCES] = {};
    float targetITDL[MAX_SOURCES]  = {};    // Target ITD from latest HRIR lookup
    float targetITDR[MAX_SOURCES]  = {};

    // Short delay lines for ITD application.
    //
    // KNOWN DEFECT, deliberately deferred (AndrewRahman/SpatialCore#25, review WR-05): the
    // delays fed to this line are the SOFA delay plus the detected onset from getAlignedHRIR,
    // so they are mostly a bulk offset, not just the ~0.7 ms (~34 samples at 48 kHz) interaural
    // difference. The measured SADIE II delays are 66 to 102 samples at 48 kHz (71 to 93 at
    // 44.1 kHz), and the read pointer masks with (kITDBufferSize - 1), so any delay of 63 or more
    // wraps modulo 64, and at 88.2 kHz and above it wraps several times. Whether the inter-ear
    // difference survives the wrap depends on both ears sitting on the same side of a multiple
    // of 64. That is the sound shipped consumers have today, so the tests pin it
    // (BinauralRendererTests "the 64-sample ITD line is characterised", D-16) and a change here is
    // a change to shipped timing that needs a decision. The intended fix: size the line from the
    // sample rate (a power of two of at least ceil (1.5e-3 * sampleRate) + 2), subtract
    // min (delayL, delayR) where the delays are derived, and update that table in the same commit.
    static constexpr int kITDBufferSize = 64;
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
