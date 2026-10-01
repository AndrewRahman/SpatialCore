#pragma once

#include <array>
#include <vector>
#include <atomic>
#include <cstring>

#include <juce_dsp/juce_dsp.h>

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/BinauralRenderer.h>
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
// single renderBlock(...) entry point, and owns the glitch-free double-buffered
// output-format switching machinery (locked IO-ownership decision — engine owns
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

    // Active output format + resolved surround layout for this block
    // (already resolved via getActiveLayout() by the consumer — the format-
    // setting API below is what POPULATES this state across blocks; the
    // per-block snapshot is still handed in explicitly to keep renderBlock
    // a pure function of its inputs for testability).
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
    // Format-setting API — owns the glitch-free double-buffered atomic-swap
    // machinery moved from OSD's OutputLayoutState/activateLayout (locked
    // IO-ownership decision: the engine owns glitch-free format switching so
    // every future consumer, e.g. OSP, gets it for free). Mirrors the
    // pre-move activateLayout(OutputFormat) + getActiveLayout() shape.
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
        std::vector<VBAPTriplet> vbapTriplets;
    };
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

    static constexpr int kRendererXfadeBlocks = 8;

private:
    //--------------------------------------------------------------------------
    // The 5 render paths (verbatim-transplanted bodies, Task 2). Internal —
    // renderBlock() is the only public entry point, matching the pre-move
    // shape where these were private OpenSpatialDelayProcessor members.
    //--------------------------------------------------------------------------
    void renderDirectBinauralHRTF (const RenderSources& sources, float* outL, float* outR, int numOutCh);
    void renderSimpleBinauralWoodworth (const RenderSources& sources, const RenderBlockContext& blockCtx,
                                         float* outL, float* outR, int numOutCh);
    void renderStereoVariant (const RenderSources& sources, const RenderBlockContext& blockCtx,
                               float* outL, float* outR, int numOutCh);
    void renderAmbisonicsOutput (const RenderSources& sources, const RenderBlockContext& blockCtx,
                                  float* const* outChannels, int numOutCh);
    void renderDiscreteSurround (const RenderSources& sources, const RenderBlockContext& blockCtx,
                                  float* const* outChannels, int numOutCh);

    void activateLayout (OutputFormat format);

    //--------------------------------------------------------------------------
    // SC-13: engine-owned gain computation, used only when the consumer sets
    // RenderBlockContext::engineComputesGains. Fills ctx.objChannelGains (via
    // surroundAlgorithm_) and ctx.objGains (via binauralAlgorithm_) for every
    // live object. Does not touch objGainL/objGainR/stereoMode (D-06 — those
    // stay consumer-side, not a SpatializationAlgorithm concern).
    //--------------------------------------------------------------------------
    void computeObjectGains (const RenderSources& sources, RenderBlockContext& ctx);

    //--------------------------------------------------------------------------
    // D-06(a), extended by D-19(ii): hold-last-good position sanitiser.
    // renderBlock copies its RenderSources into sanitizedSources_ and replaces
    // any non-finite azimuth, elevation or distance with that object's last
    // finite value, field by field (ADM-OSC forwards NaN for "field not set"
    // on single-axis updates). Finite values pass through untouched. Bodies
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
    int  rendererXfadeBlockCount_ = 0;
    int  rendererXfadeFromIdx_ = 0;
    float prevRxFadeOut_ = 1.0f;
    float prevRxFadeIn_  = 0.0f;
    std::vector<float> xfadeWetL_, xfadeWetR_;
    std::atomic<bool> rendererXfadeActive_ { false };

    // Per-source accumulation buffers for the direct-binaural HRTF pass
    // (Pass 1 -> Pass 2 hand-off), contiguous allocation mirroring the
    // pre-move sourceAccumBufPtrs[MAX_OBJECTS] shape.
    std::vector<float> sourceAccumStorage_;
    float* sourceAccumBufPtrs[MAX_SOURCES] = {};
    std::vector<float> wetBufL_, wetBufR_;

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
    //     across blocks, must live with the render path that uses it) ---
    juce::dsp::IIR::Filter<float> lfeFilter;

    // --- Output-format double-buffered layout state (glitch-free swap,
    //     moved INTO the engine per the locked IO-ownership decision) ---
    LayoutState layoutBuffers[2];
    std::atomic<int> activeLayoutIndex { 0 };
    int prepareLayoutIndex = 1;

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
