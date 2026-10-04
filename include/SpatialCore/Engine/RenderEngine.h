#pragma once

#include <array>
#include <vector>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

#include <juce_dsp/juce_dsp.h>

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SimpleBinauralCues.h>
#include <SpatialCore/Engine/TripleBufferIndex.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/BinauralRenderer.h>
#include <SpatialCore/Binaural/HRTFProfile.h>
#include <SpatialCore/IO/OutputFormat.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <SpatialCore/IO/AmbisonicsCodec.h>

namespace spatialcore
{

//==============================================================================
// RenderEngine — per-object rendering engine facade (Phase 8 Plan 08-06, CORE-01)
//
// Wraps the 5 render paths formerly living as OpenSpatialDelayProcessor member
// functions (renderDirectBinauralHRTF, renderSimpleBinauralWoodworth,
// renderStereoVariant, renderAmbisonicsOutput, renderDiscreteSurround) into a
// single renderBlock(...) entry point, and owns the glitch-free three-slot
// (wait-free TripleBufferIndex) output-format switching machinery (locked IO-ownership decision — engine owns
// format switching, consumers just call setOutputFormat(...)).
//
// ENGINE BOUNDARY (D-02, locked): the engine receives per-object mono buffers
// that are ALREADY delayed, ALREADY feedback-processed, and ALREADY
// Doppler/pitch-shifted (the OSD-owned delay-line / feedback / Doppler /
// phase-vocoder pitch-shift engine stays OUTSIDE the engine, in the consumer).
// The engine does ONLY the spatialization step: HRTF convolution, binaural/
// stereo gain panning, ambisonics SH encode + NFC-HOA, or discrete-surround
// gain application — reproducing the EXACT 5-branch dispatch structure
// currently in OpenSpatialDelayProcessor::processBlock (D-09 — do not "clean
// up" or reorder the dispatch). The engine is latency-neutral: it introduces
// no delay lines and no phase vocoder of its own; the consumer's dry-path PDC
// wraps AROUND the engine, unaffected by this facade.
//
// Source hand-off shape is a C-style array / raw-pointer flat layout
// (RenderSources, mirroring OSD's sourceAccumBufPtrs[MAX_OBJECTS] +
// objDistGain[] arrays) — the least-diff shape against the existing call
// site. This is PROVISIONAL and flagged for ergonomic review in the
// Phase 10+ post-extraction refactor (locked decision, CONTEXT.md) — do not
// redesign it now.
//
// SC-13 (opt-in): when a consumer sets RenderBlockContext::engineComputesGains,
// the engine additionally owns per-object gain computation for the
// discrete-surround/Ambisonics path (objChannelGains, via a fixed
// VBAPAlgorithm) and the simple-binaural path (objGains, via
// DirectBinauralAlgorithm + kDefaultBinauralProfiles) — the consumer no
// longer hand-builds a LayoutContext or dispatches an algorithm itself for
// those two fields. The flag defaults false, so a consumer that still
// precomputes these fields sees unchanged behaviour. Stereo-variant gains
// (objGainL/objGainR) remain consumer-side always — that math is not a
// SpatializationAlgorithm (D-06).
//
// Simple (Woodworth) binaural path (BUG-01, SpatialCore#15): the engine applies
// a position-blended rear/up/down cue bank (SimpleBinauralCues.h) to each mono
// source before the Woodworth pan gains, so sources behind, above and below the
// listener no longer sound identical to sources in front. The front half at
// ear level is bit-identical to the pre-change output, and there is no switch
// back to the flat sound (D-02).
//==============================================================================

//------------------------------------------------------------------------------
// Per-object mono source buffers + block-rate spatialization inputs, handed to
// renderBlock(...) by the consumer once per block. Every field here is a
// direct analog of a parameter/array the 5 render* methods previously read
// as either a function parameter or an OpenSpatialDelayProcessor member —
// enumerated in full in the 08-06-SUMMARY.md render-state dependency
// inventory (Pitfall 3 — nothing here may be dropped).
//------------------------------------------------------------------------------
struct RenderSources
{
    // Per-object ALREADY-delayed / ALREADY-feedback-processed / ALREADY-
    // Doppler-and-pitch-shifted mono buffers, one pointer per object slot,
    // each pointing at `numSamples` valid floats. Analog: sourceAccumBufPtrs
    // (direct-binaural path) generalized to every render path.
    const float* monoBuffers[MAX_SOURCES] = {};

    // Per-object enable/disable tap-fade envelope value, already advanced
    // per-sample by the consumer's tap-fade ramp — the engine reads this as
    // a per-sample gate/gain multiply exactly like the pre-move render
    // bodies did with tapFadeGain[t]. NOTE: per-sample values are supplied
    // via tapFadeGainPerSample below; this per-block array is unused by
    // renderBlock and retained only for symmetry with the pre-move
    // per-block enabled snapshot used to build sourceEnabled[] for the
    // direct-binaural HRTF path.
    bool sourceEnabledBlock[MAX_SOURCES] = {};

    // Per-sample tap-fade gain, one column per object, `numSamples` long,
    // pre-advanced by the consumer exactly as the old per-sample loops did
    // (tapFadeGain[t] ramped toward tapFadeTarget[t] each sample). Required
    // because every render path multiplies its spatialized output by this
    // envelope inside the per-sample loop, not just at block edges.
    const float* tapFadeGainPerSample[MAX_SOURCES] = {};

    // Per-object, per-sample distance-attenuation gain trajectory (already
    // interpolated block-start -> block-end by the consumer, matching the
    // pre-move `prevDistGain[t] + frac * (objDistGain[t] - prevDistGain[t])`
    // idiom). One pointer per object, `numSamples` long. Used by the direct-
    // binaural, ambisonics, and discrete-surround paths (NOT stereo/simple-
    // binaural, which fold distance into their own gain arrays already).
    const float* distGainPerSample[MAX_SOURCES] = {};

    // Positions this block (post-trajectory), used for HRTF HRIR updates
    // (direct-binaural) and per-sample SH coefficient computation
    // (ambisonics). Degrees, matching ObjectState.
    ObjectState objects[MAX_SOURCES] = {};

    // Whether each object slot is "live" this block (enabled OR still
    // fading out) — gates HRIR updates / SH coefficient recompute exactly
    // as the pre-move `if (objects[t].enabled)` guards did.
    bool objectLive[MAX_SOURCES] = {};

    int numSamples = 0;
};

//------------------------------------------------------------------------------
// Block-rate gain/context data pre-computed by the consumer (identical to
// what OpenSpatialDelayProcessor::processBlock computed before dispatch) and
// handed to renderBlock(...) alongside RenderSources. Kept as a separate
// struct because these are per-format-branch, not universally required per
// source, mirroring the original 5 methods' distinct parameter lists.
//------------------------------------------------------------------------------
struct RenderBlockContext
{
    // Simple (Woodworth) binaural path: current-block target gains per
    // object (DirectBinauralAlgorithm::computeBinauralGains output).
    BinauralGains objGains[MAX_SOURCES] = {};

    // Stereo-variant path: current-block target L/R gains per object
    // (computed by the consumer's stereoMode switch — VBAP/XY/MS/Blumlein/
    // Equal-Power — the gain MATH itself stays in the consumer per D-09,
    // since it is not a SpatializationAlgorithm and was never touched by
    // Plan 08-03's algorithm extraction).
    float objGainL[MAX_SOURCES] = {};
    float objGainR[MAX_SOURCES] = {};
    int   stereoMode = 0;

    // Discrete-surround path: current-block target per-speaker gains per
    // object (algorithm->computeGains output, up to MAX_SPEAKERS=16).
    float objChannelGains[MAX_SOURCES][MAX_SPEAKERS] = {};

    // Ambisonics path inputs. Sample rate is required by the NFC-HOA per-order
    // shelf-filter pole/zero recompute (matches the pre-move code's direct
    // read of currentSampleRate) — supplied per-block rather than cached at
    // prepare() time so the engine has no hidden dependency on prepare()
    // having been called with the "right" rate if it ever changes.
    int ambiOrder = 0;
    double sampleRate = 48000.0;

    // Active output format + dispatch flags for this block. By default the
    // consumer resolves these itself (the format-setting API below is what
    // POPULATES the engine's layout state across blocks; the per-block
    // snapshot is still handed in explicitly to keep renderBlock a pure
    // function of its inputs for testability). When engineDerivesDispatch is
    // set (SC-16, below), the engine overwrites activeFormat, isStereoVariant,
    // isBinaural, isAmbiOutput and ambiOrder from its own layout snapshot and
    // the consumer's values for those five fields are ignored.
    OutputFormat activeFormat = OutputFormat::Binaural;
    bool isStereoVariant = false;
    bool isBinaural = false;
    bool isAmbiOutput = false;
    bool useHRTF = false;

    // SC-13: when true, RenderEngine computes objChannelGains/objGains
    // internally (via a fixed VBAPAlgorithm for surround/Ambisonics and
    // DirectBinauralAlgorithm for simple binaural) before dispatch, instead
    // of reading consumer-precomputed values. Defaults false so every
    // existing caller — including SpatialCore's own RenderEngineTests — sees
    // byte-for-byte unchanged behaviour; this is additive, not a major bump.
    // Scoped strictly to objChannelGains/objGains: objGainL/objGainR (stereo-
    // variant gains) stay consumer-side per the comment above (D-06).
    bool engineComputesGains = false;

    // SC-16: when true, renderBlock() acquires the engine's layout once for
    // the block and derives activeFormat, isStereoVariant, isBinaural,
    // isAmbiOutput and ambiOrder from that snapshot's format, so the dispatch
    // and the layout it renders against can never come from two different
    // output-format switches. The consumer's values for those five fields are
    // ignored. useHRTF, stereoMode, sampleRate, objGainL and objGainR stay
    // consumer-supplied. Defaults false so every existing caller — including
    // SpatialCore's own RenderEngineTests — sees byte-for-byte unchanged
    // behaviour; this is additive, not a major bump.
    bool engineDerivesDispatch = false;

    // D-15, engineSelectsHRTF: when true, renderBlock() sets useHRTF itself from its own active renderer
    // (useHRTF = ! isSimpleMode()) after claiming any ready profile, and the consumer's
    // useHRTF is ignored. While a switch between Simple (profile 0) and an HRTF profile is
    // fading, the engine renders the Woodworth path and the HRTF path and blends them with
    // the renderer crossfade's equal-power ramp, so a Simple <-> HRTF switch is as click-free
    // as an HRTF <-> HRTF one. The Woodworth path may therefore run during an HRTF fade and
    // reads objGains, so on a binaural, non-stereo-variant block this flag also makes the engine
    // compute objGains itself, as engineComputesGains does (WR-06): no flag combination can leave
    // the Simple path running on zero gains. objGains you supply on such a block are overwritten;
    // objChannelGains stay yours unless you also set engineComputesGains.
    // Only a binaural, non-stereo-variant block is affected; every other format renders as it
    // always did. Defaults false so every existing caller -- including OpenSpatialDelay's
    // derive-useHRTF-itself flow -- sees unchanged behaviour from this flag. It is NOT
    // byte-for-byte unchanged against the pre-Phase-3 engine: three changes apply to every
    // consumer regardless of any flag, namely the Simple path's rear/up/down cue bank (BUG-01),
    // the sample-based renderer crossfade (kMinRendererXfadeSamples) and the convolver warm-up
    // that now waits for one IR length of samples (see docs/integration-guide.md, "Fades and
    // warm-up are timed in samples"). The flag itself is additive, not a major bump.
    bool engineSelectsHRTF = false;
};

//==============================================================================
// EngineState — every implicit render-state dependency the 5 render* methods
// read or wrote as OpenSpatialDelayProcessor members, now owned by the engine
// instance instead (Pitfall 3 — every one of these is REQUIRED, none dropped).
// This is gain-interpolation / renderer / layout state that persists block to
// block, as opposed to RenderSources/RenderBlockContext above which are
// re-supplied fresh every renderBlock(...) call.
//==============================================================================
class RenderEngine
{
public:
    RenderEngine();
    ~RenderEngine();

    //--------------------------------------------------------------------------
    // Prepare engine-owned DSP state for a given sample rate / block size —
    // analog of the slice of OpenSpatialDelayProcessor::prepareToPlay that
    // touched engine-owned state: lfeFilter.prepare()/reset() + 120 Hz
    // low-pass coefficient set, and sizing the direct-binaural HRTF pass's
    // per-source accumulation + wet buffers. Does NOT touch delay lines,
    // phase vocoder, or any other OSD-owned DSP (out of the engine boundary).
    //
    // Thread contract (WR-04): call it from the same thread as setHRTFProfile()
    // (the message thread; JUCE's prepareToPlay on a host that calls it elsewhere
    // must not overlap a setHRTFProfile() call), and never while renderBlock() is
    // running. Once a profile has been requested, prepare() stops the engine's HRTF
    // loader thread and starts a new one, so it BLOCKS until a load already in
    // flight finishes (a built-in file is up to 36 MB, a shared-folder file up to
    // kMaxSharedHRTFFileBytes, 256 MB; the wait gives up after 15 s). It then
    // re-issues the current request; an unclaimed result from before the call is
    // discarded. An engine that never had a profile requested starts no thread
    // and never blocks.
    //--------------------------------------------------------------------------
    void prepare (double sampleRate, int maxBlockSize);

    //--------------------------------------------------------------------------
    // Per-block entry point. Reproduces the EXACT 5-branch dispatch structure
    // from the pre-move OpenSpatialDelayProcessor::processBlock (D-09 — do
    // not reorder or simplify):
    //   isStereoVariant                -> renderStereoVariant
    //   isBinaural && useHRTF          -> renderDirectBinauralHRTF
    //   isBinaural                     -> renderSimpleBinauralWoodworth
    //   isAmbiOutput                   -> renderAmbisonicsOutput
    //   else                           -> renderDiscreteSurround
    //
    // outL/outR/outChannels: raw write pointers into the consumer's output
    // buffer for this block (same shape as juce::AudioBuffer::getWritePointer
    // results in the pre-move code) — the engine does NOT own or allocate the
    // output buffer. numOutCh is the number of valid pointers in outChannels.
    //
    // Writes RAW WET signal only — no dry/wet mix, no output-gain, no
    // limiter, no dry-path PDC (all of that stays in the consumer, applied
    // once after renderBlock returns, exactly as processBlock's post-dispatch
    // single-point dry/wet mix block does today).
    //--------------------------------------------------------------------------
    void renderBlock (const RenderSources& sources,
                       const RenderBlockContext& blockCtx,
                       float* const* outChannels,
                       int numOutCh);

    //--------------------------------------------------------------------------
    // Format-setting API — owns the glitch-free three-slot layout handoff
    // (a wait-free TripleBufferIndex, SC-16) that replaced the two-slot swap
    // moved from OSD's OutputLayoutState/activateLayout (locked IO-ownership
    // decision: the engine owns glitch-free format switching so every future
    // consumer, e.g. OSP, gets it for free).
    //
    // setOutputFormat() thread contract:
    //   - message thread only, and single-writer: never call it from two
    //     threads at once. The engine does NOT serialise writers: a consumer
    //     with more than one non-realtime caller (for example a UI handler
    //     and the host's prepare callback, which some hosts run on a worker
    //     thread) must serialise them itself, with a mutex or by funnelling
    //     them onto one thread. Two overlapping calls fill the same layout
    //     slot concurrently, which is undefined behaviour. Debug builds
    //     jassert on overlapping entry;
    //   - allocating (it builds the speaker layout, decode matrix and VBAP
    //     triplets), so never from the audio thread;
    //   - safe to call any number of times between two blocks: the layout is
    //     filled in a slot the audio thread does not hold, superseded calls are
    //     skipped whole, and the next block renders the last call.
    //
    // getActiveOutputFormat() / getActiveLayout() are the WRITER-thread view:
    // the most recently published layout, callable only from the thread that
    // calls setOutputFormat(), never from the audio thread.
    //--------------------------------------------------------------------------
    void setOutputFormat (OutputFormat format);
    OutputFormat getActiveOutputFormat() const;

    //--------------------------------------------------------------------------
    // SC-13: selects which of kDefaultBinauralProfiles the engine-owned
    // simple-binaural gain computation uses when engineComputesGains is set.
    // Message-thread only, matching DirectBinauralAlgorithm.cpp's existing
    // index convention (1-based, clamped to 0..4 internally via index - 1).
    //--------------------------------------------------------------------------
    void setBinauralProfileIndex (int index) { binauralProfileIndex_ = index; }

    struct LayoutState
    {
        OutputFormat format = OutputFormat::Binaural;
        SpeakerLayout layout {};
        float ambiDecodeMatrix[MAX_SPEAKERS][MAX_SPEAKERS] = {};
        int ambiNumSpeakers = 0;
        // buildVBAPTripletsForLayout's regular triplets, then
        // appendLowerHemisphereTriplets' nadir caps and pair-pan wedges
        // (VBAPTriplet::kind(); empty for flat and non-speaker formats).
        std::vector<VBAPTriplet> vbapTriplets;
    };
    // Writer-thread view: the most recently published layout, callable only
    // from the thread that calls setOutputFormat(). Not a per-block snapshot —
    // the audio thread obtains its layout once per block inside renderBlock()
    // (SC-16) and must never use this accessor.
    const LayoutState& getActiveLayout() const;

    //--------------------------------------------------------------------------
    // Escape hatches (D-02 — leaf classes stay public). Consumers that need
    // direct access to the underlying renderer/HRTF database (e.g. for
    // profile loading, synchronous test setup per the CLAUDE.md gotcha)
    // reach through here rather than the engine reimplementing a proxy API.
    //--------------------------------------------------------------------------
    BinauralRenderer& getBinauralRenderer (int index) { return binauralRenderers[static_cast<size_t> (index)]; }
    std::atomic<int>& getActiveRendererIndexAtomic() { return activeRendererIndex; }

    // HRTF profile double-buffered swap escape hatches — message-thread-only
    // (OSD's timerCallback()/loadHRTFProfile() reach through these; the
    // prepare-index bookkeeping mirrors setOutputFormat()'s layout swap
    // exactly, but the profile-load body itself (SOFA file IO via
    // HRTFDatabase::loadFromFile) is OSD-specific glue code, not engine
    // logic, so it stays a consumer-side call using getBinauralRenderer()).
    int getPrepareRendererIndex() const { return prepareRendererIndex_; }
    void swapActiveRenderer()
    {
        activeRendererIndex.store (prepareRendererIndex_, std::memory_order_release);
        prepareRendererIndex_ = 1 - prepareRendererIndex_;
    }
    bool isRendererCrossfadeActive() const { return rendererXfadeActive_.load (std::memory_order_acquire); }

    // Renderer crossfade length (a profile swap). Counted in elapsed SAMPLES, fixed when the
    // fade starts: max (kRendererXfadeBlocks x the block size at that moment,
    // kMinRendererXfadeSamples). A pure block count made the fade 8 x 32 = 256 samples long at
    // 32-sample blocks, shorter than the HRIRs it blends; the sample floor keeps it at least 85 ms
    // at 48 kHz at any block size. At 512-sample blocks and above it is the old 8-block fade.
    static constexpr int kRendererXfadeBlocks = 8;
    static constexpr int kMinRendererXfadeSamples = 4096;

    //--------------------------------------------------------------------------
    // Engine-owned HRTF profile switching (DATA-01, D-04, D-05, D-06).
    //
    // One call replaces the consumer's load / swap / timer glue. setHRTFProfile()
    // only stores the request and wakes one engine-owned worker thread; the worker
    // resolves the profile (shared folder, then the embedded copy, then a reported
    // error), loads it into the renderer the audio thread is not using, and hands
    // it over through a one-slot atomic mailbox. The audio thread claims it at the
    // top of a block and crossfades with the existing renderer crossfade. Audio
    // keeps playing the current profile until the new one is ready, and a failed
    // load never mutes anything: the current profile keeps playing and the status
    // says what went wrong.
    //
    // Thread contract:
    //   - setHRTFProfile(), waitForHRTFProfileIdle(), setSharedHRTFFolderForTesting():
    //     message thread (or the single test thread). Never the audio thread. prepare()
    //     shares the loader-thread bookkeeping with setHRTFProfile() and must not overlap it
    //     (see prepare() above); nothing here is guarded by a lock, by design, so the
    //     message thread is never made to wait on a join.
    //   - getHRTFProfileStatus(): any thread, lock-free.
    //   - The SOFA load (up to 36 MB built in, up to 256 MB
    //     from the shared folder) runs on the worker only. setHRTFProfile()
    //     returns at once. The worker is started by the first request after
    //     prepare(); an engine that never gets a request starts no thread.
    //   - Profile numbering is HRTFProfile.h's: 0 Simple, 1..5 the SOFA profiles.
    //
    // Most recent request wins. Requests made before the first prepare() are held
    // and served after it. Requesting the profile that is already active and
    // settled does nothing; requesting a profile whose last load failed retries it.
    //
    // Mixing these calls with swapActiveRenderer() / getPrepareRendererIndex() /
    // getBinauralRenderer() loads on one engine is unsupported: pick one way.
    //--------------------------------------------------------------------------
    void setHRTFProfile (int profileIndex);

    /** Lock-free snapshot, safe from any thread. Use describeHRTFProfileStatus()
        (allocates) on the reader's own thread to turn it into text. */
    HRTFProfileStatus getHRTFProfileStatus() const noexcept;

    /** True once no request is pending and no load is running (it does not wait for
        the audio thread to claim the result). Message or test thread only. */
    bool waitForHRTFProfileIdle (int timeoutMs);

    /** Overrides getSharedHRTFFolder() for this engine (the real folder is root-owned
        and shared by the whole machine). Message thread, before or between requests. */
    void setSharedHRTFFolderForTesting (const juce::File& folder);

    /** TEST-ONLY. Not part of the consumer API: a plugin must never call it. While enabled,
        the worker throws std::bad_alloc where a profile load would start, so a test can
        exercise the failure path that an out-of-memory load takes: the request settles as
        Failed / HRTFProfileProblem::LoadFailed and the current profile keeps playing. Any
        thread; the default is off. It is declared unconditionally, like
        setSharedHRTFFolderForTesting(), because a compile guard would change the class layout
        between SpatialCore's own build and a consumer's, which is an ODR hazard. */
    void setLoaderFailureForTesting (bool enabled);

private:
    class HRTFProfileLoader;   // defined in RenderEngine.cpp; a nested class sees the private state below

    //--------------------------------------------------------------------------
    // The 5 render paths (verbatim-transplanted bodies, Task 2). Internal —
    // renderBlock() is the only public entry point, matching the pre-move
    // shape where these were private OpenSpatialDelayProcessor members.
    //--------------------------------------------------------------------------
    // activeIdx is the one acquire-load of activeRendererIndex for this block. With simpleCtx
    // null this is the legacy path, bit for bit: both renderers are treated as HRTF renderers.
    // With simpleCtx set (engineSelectsHRTF, D-15) it also handles a fade in which exactly one
    // side is a Simple (profile 0) renderer: that side is the Woodworth path, run once into
    // simpleWetL_/simpleWetR_. Returns true when the Woodworth path ran this block.
    bool renderDirectBinauralHRTF (const RenderSources& sources, float* outL, float* outR, int numOutCh,
                                    int activeIdx, const RenderBlockContext* simpleCtx = nullptr);
    // D-15: the one function a binaural block goes through when engineSelectsHRTF is set. Plain
    // Simple and plain HRTF (including HRTF <-> HRTF) call exactly the path the flag-off dispatch
    // would, so the output is bit-identical; only a Simple <-> HRTF fade blends the two paths.
    // Returns true when the Woodworth path ran this block.
    bool renderBinauralWithProfileFade (const RenderSources& sources, const RenderBlockContext& ctx,
                                         float* outL, float* outR, int numOutCh);
    // Starts the renderer crossfade when the active renderer changed since the last block.
    void beginRendererFadeIfSwapped (int activeIdx, int numSamples);
    // Ends the renderer crossfade and hands the faded-out renderer back to the loader.
    void endRendererFade();
    // Simple binaural path: applies the position-blended rear/up/down cue bank
    // from SimpleBinauralCues.h before the Woodworth pan gains (BUG-01); the
    // front half at ear level is unchanged. Cue filter state is engine-owned
    // and zeroed when this path resumes after another one.
    void renderSimpleBinauralWoodworth (const RenderSources& sources, const RenderBlockContext& blockCtx,
                                         float* outL, float* outR, int numOutCh);
    void renderStereoVariant (const RenderSources& sources, const RenderBlockContext& blockCtx,
                               float* outL, float* outR, int numOutCh);
    void renderAmbisonicsOutput (const RenderSources& sources, const RenderBlockContext& blockCtx,
                                  float* const* outChannels, int numOutCh);
    void renderDiscreteSurround (const RenderSources& sources, const RenderBlockContext& blockCtx,
                                  const LayoutState& layout,
                                  float* const* outChannels, int numOutCh);

    void activateLayout (OutputFormat format);

    //--------------------------------------------------------------------------
    // SC-13: engine-owned gain computation, used only when the consumer sets
    // RenderBlockContext::engineComputesGains. Fills ctx.objChannelGains (via
    // surroundAlgorithm_) and ctx.objGains (via binauralAlgorithm_) for every
    // live object. Does not touch objGainL/objGainR/stereoMode (D-06 — those
    // stay consumer-side, not a SpatializationAlgorithm concern).
    //--------------------------------------------------------------------------
    // With binauralOnly set (engineSelectsHRTF without engineComputesGains, WR-06) only
    // ctx.objGains is filled and objChannelGains is left as the consumer supplied it.
    void computeObjectGains (const RenderSources& sources, const LayoutState& layout,
                              RenderBlockContext& ctx, bool binauralOnly = false);

    //--------------------------------------------------------------------------
    // SC-16: the one place a block obtains its layout. renderBlock() calls
    // this exactly once and passes the same snapshot to the dispatch
    // derivation, the SC-13 gain computation and the discrete-surround
    // speaker routing; no render path reads the active-layout state again.
    //--------------------------------------------------------------------------
    const LayoutState& acquireBlockLayout();

    // SC-16: overwrites the five dispatch fields in ctx from layout.format.
    // The format is clamped into [0, NUM_OUTPUT_FORMATS - 1] before it is
    // used as an OutputFormatRegistry index (the registry subscript has no
    // bounds check of its own).
    static void deriveDispatchFromLayout (const LayoutState& layout, RenderBlockContext& ctx);

    //--------------------------------------------------------------------------
    // D-06(a), extended by D-19(ii): hold-last-good position sanitiser.
    // renderBlock copies its RenderSources into sanitizedSources_ and replaces
    // any non-finite azimuth, elevation or distance with that object slot's
    // last finite value, field by field (ADM-OSC forwards NaN for "field not
    // set" on single-axis updates). Finite values pass through untouched. The
    // held values belong to the slot (IN-04): every slot updates every block,
    // live or not, and only the constructor and prepare() reset them. Bodies
    // live in RenderEngine.cpp so the non-finite checks cannot be folded
    // away by a consumer's -ffast-math (RESEARCH F9).
    //--------------------------------------------------------------------------
    const RenderSources& sanitizeSources (const RenderSources& sources);
    void resetLastGoodPositions();

    // ACN channel index -> SH order lookup (verbatim from
    // OpenSpatialDelayProcessor::acnToOrder, moved because it is used only by
    // renderAmbisonicsOutput's max-rE weighting).
    static constexpr int acnToOrder (int acn)
    {
        if (acn < 1)  return 0;  if (acn < 4)  return 1;
        if (acn < 9)  return 2;  if (acn < 16) return 3;
        if (acn < 25) return 4;  if (acn < 36) return 5;
        return 6;
    }

    //==========================================================================
    // ENGINE-OWNED STATE — every field below is a required carry-forward of
    // an implicit render-state dependency found in the Task 1 inventory
    // (see 08-06-SUMMARY.md for the full per-method breakdown). None of these
    // may be dropped or the crossfade-pop bug classes (Spatial-Media-Lab/OpenSpatialDelay#90, Spatial-Media-Lab/OpenSpatialDelay#96) reappear.
    //==========================================================================

    // --- Direct-binaural HRTF renderer state (double-buffered for
    //     glitch-free profile swap, plus crossfade-on-swap state) ---
    BinauralRenderer binauralRenderers[2];
    std::atomic<int> activeRendererIndex { 0 };
    int  prepareRendererIndex_ = 1;
    int  prevActiveRendererIdx_ = 0;
    bool rendererXfading_ = false;
    int  rendererXfadeSamplesDone_ = 0;     // samples elapsed since the fade started
    int  rendererXfadeLengthSamples_ = 0;   // fixed when the fade starts
    int  rendererXfadeFromIdx_ = 0;
    float prevRxFadeOut_ = 1.0f;
    float prevRxFadeIn_  = 0.0f;
    std::vector<float> xfadeWetL_, xfadeWetR_;
    // D-15: the Woodworth path's output during a Simple <-> HRTF fade. Sized in prepare() like
    // the buffers above, so the fade allocates nothing on the audio thread (DR-1).
    std::vector<float> simpleWetL_, simpleWetR_;
    std::atomic<bool> rendererXfadeActive_ { false };

    // --- Engine-owned profile switching (see setHRTFProfile above). The worker
    //     only ever writes a renderer that is neither the active one nor the
    //     source of a running crossfade: a free renderer, or its own unclaimed
    //     result. The audio thread is the only writer of activeRendererIndex on
    //     this path and only reads/writes the atomics below (DR-1). ---
    std::unique_ptr<HRTFProfileLoader> hrtfLoader_;
    std::atomic<int>      requestedProfile_ { 0 };
    std::atomic<uint32_t> requestSerial_ { 0 };      // bumped by setHRTFProfile
    std::atomic<uint32_t> handledSerial_ { 0 };      // set by the worker once a request is settled
    std::atomic<int>      readyRenderer_ { -1 };     // one-slot mailbox: worker -> audio thread
    std::atomic<bool>     rendererFree_[2] { { false }, { true } };   // audio thread -> worker
    std::atomic<uint32_t> rendererMeta_[2] { { 0u }, { 0u } };        // what each renderer was loaded from
    std::atomic<int>      activeHRTFProfile_ { 0 };
    std::atomic<uint32_t> loaderStatusWord_ { 0u };
    std::atomic<bool>     loaderBusy_ { false };
    std::atomic<bool>     forceReload_ { false };
    std::atomic<bool>     loaderThrowForTesting_ { false };   // TEST-ONLY hook, see setLoaderFailureForTesting()
    double preparedSampleRate_ = 0.0;
    int    preparedMaxBlock_ = 0;
    bool   hrtfEverRequested_ = false;               // message thread only

    mutable juce::CriticalSection sharedFolderLock_;         // message and worker threads only
    juce::File sharedFolderOverride_;
    bool hasSharedFolderOverride_ = false;

    /** Audio thread, once per block after the dispatch context is final. Takes a ready
        renderer from the mailbox, and ends a crossfade stranded by a path change. */
    void claimReadyRenderer (bool hrtfPathThisBlock);
    juce::File getSharedFolderForLoader() const;

    // Per-source accumulation buffers for the direct-binaural HRTF pass
    // (Pass 1 -> Pass 2 hand-off), contiguous allocation mirroring the
    // pre-move sourceAccumBufPtrs[MAX_OBJECTS] shape.
    std::vector<float> sourceAccumStorage_;
    float* sourceAccumBufPtrs[MAX_SOURCES] = {};
    std::vector<float> wetBufL_, wetBufR_;

    // --- Simple-path cue bank (BUG-01, SpatialCore#15, D-01). Three fixed
    //     filter branches (rear / up / down, tables in SimpleBinauralCues.h)
    //     designed in prepare() for the actual sample rate and run on the mono
    //     source before the Woodworth pan gains. Fixed-size members only: no
    //     allocation, lock or logging on the audio thread (DR-1). Biquads are
    //     transposed direct form II, state per source slot. Default
    //     coefficients are identity so an unprepared engine passes the signal
    //     through. ---
    struct CueBiquad { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };
    CueBiquad cueRear_[3];
    CueBiquad cueUp_[3];
    CueBiquad cueDown_[2];
    float cueStateRear_[MAX_SOURCES][3][2] = {};
    float cueStateUp_[MAX_SOURCES][3][2] = {};
    float cueStateDown_[MAX_SOURCES][2][2] = {};
    SimpleCueWeights prevCueWeights_[MAX_SOURCES] = {};
    // True when the previous block ran renderSimpleBinauralWoodworth. When it
    // did not, the cue state is stale (minutes-old audio possibly), so the
    // Simple path zeroes it and starts the weights at their targets on resume.
    bool simplePathRanLastBlock_ = false;

    void designSimpleCueBank (double sampleRate);
    void resetSimpleCueState();

    // --- Gain-interpolation carry-forward state (per render path) ---
    // THE highest-risk field in this entire engine: dropping this
    // reintroduces the exact crossfade-pop class the harness's
    // rapid-position-change configs exist to catch.
    // NOTE: distance-gain (objDistGain) interpolation is NOT engine state —
    // the consumer pre-interpolates it into RenderSources::distGainPerSample
    // (already a per-sample array) before calling renderBlock(), since the
    // pre-move code computed it identically across 3 of the 5 render paths
    // from a single `prevDistGain` the consumer owns. Only per-path GAIN
    // interpolation (binaural/stereo/surround-channel/SH-coefficient) is
    // engine-owned, since those targets are format-branch-specific.
    BinauralGains prevBinauralGains[MAX_SOURCES] = {};
    float prevStereoGainL[MAX_SOURCES] = {};
    float prevStereoGainR[MAX_SOURCES] = {};
    float prevChannelGains[MAX_SOURCES][MAX_SPEAKERS] = {};

    // --- Ambisonics NFC-HOA + max-rE weighting state ---
    static constexpr int kMaxAmbiOrder = 6;
    static constexpr int kMaxAmbiChannels = (kMaxAmbiOrder + 1) * (kMaxAmbiOrder + 1);
    static constexpr float kNfcReferenceRadius = 1.5f; // meters (typical studio monitoring distance)
    float prevSHCoeffs[MAX_SOURCES][kMaxAmbiChannels] = {};
    juce::dsp::IIR::Filter<float> nfcFilters[MAX_SOURCES][kMaxAmbiOrder]; // 12 objects x 6 orders
    float smoothedNfcDistance[MAX_SOURCES] = {};
    float prevNfcDistance[MAX_SOURCES] = {};
    int   cachedMaxrEOrder = -1;
    float cachedMaxrE[kMaxAmbiOrder + 1] = {};

    // --- Discrete-surround LFE generation filter (stateful IIR — persists
    //     across blocks, must live with the render path that uses it).
    //     Double precision: a float32 120 Hz biquad enters a platform-dependent
    //     rounding limit cycle (see prepare()). ---
    juce::dsp::IIR::Filter<double> lfeFilter;

    // --- Output-format layout state (glitch-free three-slot handoff, moved
    //     INTO the engine per the locked IO-ownership decision). The message
    //     thread fills layoutBuffers[layoutSlots_.writeSlot()] then publishes;
    //     the audio thread reads layoutBuffers[layoutSlots_.acquireLatest()]
    //     once per block. The writer never holds the reader's slot (SC-16). ---
    LayoutState layoutBuffers[TripleBufferIndex::kNumSlots];
    TripleBufferIndex layoutSlots_;
    // Debug detector for the single-writer contract (WR-09): set while a
    // setOutputFormat() call is in flight so an overlapping second writer
    // trips a jassert. Present in every build type (never #if'd) so the class
    // layout does not depend on the consumer's JUCE_DEBUG setting.
    std::atomic<bool> inSetOutputFormat_ { false };

    // --- SC-13: engine-owned gain computation state ---
    // Algorithms are stateless per the project convention, so a plain member
    // instance allocates nothing and is safe to call from the audio thread.
    // Fixed to VBAP/DirectBinaural deliberately: runtime algorithm selection
    // is SPAT-01 (a later, separate concern) and must not be pulled forward.
    VBAPAlgorithm surroundAlgorithm_;
    DirectBinauralAlgorithm binauralAlgorithm_;
    RenderBlockContext gainScratch_;
    int binauralProfileIndex_ = 1;

    // --- D-06(a) / D-19(ii): hold-last-good position state (preallocated,
    //     written only by sanitizeSources on the audio thread; reset to the
    //     ObjectState defaults 0, 0, 0.5 by the constructor and prepare()) ---
    RenderSources sanitizedSources_;
    float lastGoodAzimuthDeg_[MAX_SOURCES] = {};
    float lastGoodElevationDeg_[MAX_SOURCES] = {};
    float lastGoodDistance_[MAX_SOURCES] = {};
};

} // namespace spatialcore
