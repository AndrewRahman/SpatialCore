#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>
#include "../Binaural/BinauralTestUtilities.h"
#include "../TestNumerics.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// RenderEngine — mini render-and-compare integration tests (Phase 8 Plan
// 08-06, CORE-01). These are NOT bit-exactness proofs against the monolith
// (that gate is Tests/RegressionHarness in the OSD consumer repo) -- they
// exercise the facade's own contract: renderBlock() dispatches to the
// correct of the 5 branches, produces finite non-silent output for a live
// source, stays silent for a fully-faded (tapFadeGainPerSample == 0) source,
// and setOutputFormat()'s double-buffered swap becomes visible via
// getActiveLayout()/getActiveOutputFormat(). Committed as a SEPARATE commit
// from the render-path transplant itself (D-09).
// ============================================================================

namespace
{
    constexpr int kBlockSize = 64;
    constexpr double kSampleRate = 48000.0;

    // Builds a RenderSources block with a single live object at slot 0,
    // constant unity mono signal, full tap-fade gain, and unity distance gain
    // -- the simplest possible "one source, fully audible" configuration.
    struct SourceFixture
    {
        std::vector<float> mono   = std::vector<float> (kBlockSize, 0.5f);
        std::vector<float> tapFade = std::vector<float> (kBlockSize, 1.0f);
        std::vector<float> distGain = std::vector<float> (kBlockSize, 1.0f);
        std::vector<float> silentTapFade = std::vector<float> (kBlockSize, 0.0f);

        RenderSources makeSources (bool live = true, bool silent = false)
        {
            RenderSources s;
            s.numSamples = kBlockSize;
            s.monoBuffers[0] = mono.data();
            s.tapFadeGainPerSample[0] = silent ? silentTapFade.data() : tapFade.data();
            s.distGainPerSample[0] = distGain.data();
            s.objectLive[0] = live;
            s.objects[0].azimuthDeg = 30.0f;
            s.objects[0].elevationDeg = 0.0f;
            s.objects[0].distance = 0.5f;
            s.objects[0].enabled = live;
            return s;
        }
    };

    bool allFinite (const float* buf, int n)
    {
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (buf[i]))
                return false;
        return true;
    }

    bool anyNonzero (const float* buf, int n)
    {
        for (int i = 0; i < n; ++i)
            if (buf[i] != 0.0f)
                return true;
        return false;
    }

    bool allZero (const float* buf, int n)
    {
        for (int i = 0; i < n; ++i)
            if (buf[i] != 0.0f)
                return false;
        return true;
    }
}

// ----------------------------------------------------------------------------
// Direct-binaural HRTF branch (isBinaural && useHRTF)
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: direct-binaural HRTF branch produces finite non-silent stereo output",
           "[engine][renderblock][binaural-hrtf]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Binaural);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.isBinaural = true;
    ctx.useHRTF = true;

    std::vector<float> outL (kBlockSize, 0.0f), outR (kBlockSize, 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };

    // Two blocks: the binaural renderer defaults to Simple/Woodworth mode
    // until a profile is loaded, so this exercises renderDirectBinauralHRTF's
    // dispatch + gain-interpolation-state bookkeeping without requiring a
    // SOFA file load (out of scope for this facade-contract test).
    engine.renderBlock (sources, ctx, outPtrs, 2);
    engine.renderBlock (sources, ctx, outPtrs, 2);

    CHECK (allFinite (outL.data(), kBlockSize));
    CHECK (allFinite (outR.data(), kBlockSize));
}

// ----------------------------------------------------------------------------
// Direct-binaural HRTF branch — oversized block (CR-03 regression)
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: direct-binaural HRTF branch survives an oversized block without heap corruption",
           "[engine][renderblock][oversized]")
{
    // Prepare the engine at the SMALL fixture block size, then render a
    // block whose numSamples is 4x larger than what was prepared — the
    // exact prepare-contract violation CR-03 identified. Pre-fix, this
    // overflowed sourceAccumStorage_ (fixed at MAX_SOURCES * kBlockSize)
    // for source MAX_SOURCES-1 and corrupted adjacent per-source slices for
    // every other live source. This test is designed to trip ASan/heap
    // guards on the pre-fix engine and pass cleanly post-fix.
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Binaural);

    constexpr int kOversizedNumSamples = kBlockSize * 4;

    std::vector<float> mono (kOversizedNumSamples, 0.5f);
    std::vector<float> tapFade (kOversizedNumSamples, 1.0f);
    std::vector<float> distGain (kOversizedNumSamples, 1.0f);

    RenderSources sources;
    sources.numSamples = kOversizedNumSamples;
    sources.monoBuffers[0] = mono.data();
    sources.tapFadeGainPerSample[0] = tapFade.data();
    sources.distGainPerSample[0] = distGain.data();
    sources.objectLive[0] = true;
    sources.objects[0].azimuthDeg = 30.0f;
    sources.objects[0].elevationDeg = 0.0f;
    sources.objects[0].distance = 0.5f;
    sources.objects[0].enabled = true;

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.isBinaural = true;
    ctx.useHRTF = true;

    std::vector<float> outL (kOversizedNumSamples, 0.0f), outR (kOversizedNumSamples, 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };

    engine.renderBlock (sources, ctx, outPtrs, 2);
    engine.renderBlock (sources, ctx, outPtrs, 2);

    CHECK (allFinite (outL.data(), kOversizedNumSamples));
    CHECK (allFinite (outR.data(), kOversizedNumSamples));
}

// ----------------------------------------------------------------------------
// Simple (Woodworth) binaural branch (isBinaural && !useHRTF)
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: simple Woodworth binaural branch produces non-silent finite output for a live source",
           "[engine][renderblock][binaural-simple]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Binaural);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.isBinaural = true;
    ctx.useHRTF = false;
    ctx.objGains[0].leftGain = 0.8f;
    ctx.objGains[0].rightGain = 0.6f;

    std::vector<float> outL (kBlockSize, 0.0f), outR (kBlockSize, 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };

    engine.renderBlock (sources, ctx, outPtrs, 2);

    CHECK (allFinite (outL.data(), kBlockSize));
    CHECK (allFinite (outR.data(), kBlockSize));
    CHECK (anyNonzero (outL.data(), kBlockSize));
    CHECK (anyNonzero (outR.data(), kBlockSize));
}

TEST_CASE ("RenderEngine: simple Woodworth binaural branch is silent when tap-fade gain is zero",
           "[engine][renderblock][binaural-simple][silence]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Binaural);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources (/*live*/ true, /*silent*/ true);

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.isBinaural = true;
    ctx.useHRTF = false;
    ctx.objGains[0].leftGain = 0.8f;
    ctx.objGains[0].rightGain = 0.6f;

    std::vector<float> outL (kBlockSize, 0.0f), outR (kBlockSize, 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };

    engine.renderBlock (sources, ctx, outPtrs, 2);

    CHECK (allZero (outL.data(), kBlockSize));
    CHECK (allZero (outR.data(), kBlockSize));
}

// ----------------------------------------------------------------------------
// Stereo-variant branch (isStereoVariant)
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: stereo-variant branch produces non-silent finite output for a live source",
           "[engine][renderblock][stereo]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Stereo);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.isStereoVariant = true;
    ctx.stereoMode = 0; // Equal Power
    ctx.objGainL[0] = 0.7f;
    ctx.objGainR[0] = 0.3f;

    std::vector<float> outL (kBlockSize, 0.0f), outR (kBlockSize, 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };

    engine.renderBlock (sources, ctx, outPtrs, 2);

    CHECK (allFinite (outL.data(), kBlockSize));
    CHECK (allFinite (outR.data(), kBlockSize));
    CHECK (anyNonzero (outL.data(), kBlockSize));
    CHECK (anyNonzero (outR.data(), kBlockSize));
}

// ----------------------------------------------------------------------------
// Ambisonics branch (isAmbiOutput)
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: ambisonics branch produces finite output on the W channel (order 0)",
           "[engine][renderblock][ambisonics]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::AmbisonicsFOA);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.isAmbiOutput = true;
    ctx.ambiOrder = 1; // FOA

    constexpr int kFoaChannels = 4; // (1+1)^2
    std::vector<std::vector<float>> outStorage (kFoaChannels, std::vector<float> (kBlockSize, 0.0f));
    float* outPtrs[kFoaChannels] = {};
    for (int c = 0; c < kFoaChannels; ++c)
        outPtrs[c] = outStorage[static_cast<size_t> (c)].data();

    engine.renderBlock (sources, ctx, outPtrs, kFoaChannels);

    for (int c = 0; c < kFoaChannels; ++c)
        CHECK (allFinite (outPtrs[c], kBlockSize));

    // W channel (ACN 0) always carries signal for a live omni-ish source.
    CHECK (anyNonzero (outPtrs[0], kBlockSize));
}

// ----------------------------------------------------------------------------
// Discrete-surround branch (else)
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: discrete-surround branch produces finite output and respects speaker channel routing",
           "[engine][renderblock][surround]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Quad);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();

    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    // isStereoVariant/isBinaural/isAmbiOutput all false -> falls through to
    // renderDiscreteSurround, matching the pre-move dispatch's final `else`.
    ctx.objChannelGains[0][0] = 1.0f; // full gain to speaker 0

    constexpr int kMaxCh = 8;
    std::vector<std::vector<float>> outStorage (kMaxCh, std::vector<float> (kBlockSize, 0.0f));
    float* outPtrs[kMaxCh] = {};
    for (int c = 0; c < kMaxCh; ++c)
        outPtrs[c] = outStorage[static_cast<size_t> (c)].data();

    engine.renderBlock (sources, ctx, outPtrs, kMaxCh);

    for (int c = 0; c < kMaxCh; ++c)
        CHECK (allFinite (outPtrs[c], kBlockSize));
}

// ----------------------------------------------------------------------------
// setOutputFormat() — glitch-free double-buffered swap visibility
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: setOutputFormat swap becomes visible via getActiveOutputFormat/getActiveLayout",
           "[engine][format-switch]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    engine.setOutputFormat (OutputFormat::Binaural);
    CHECK (engine.getActiveOutputFormat() == OutputFormat::Binaural);
    CHECK (engine.getActiveLayout().format == OutputFormat::Binaural);

    engine.setOutputFormat (OutputFormat::Surround5_1);
    CHECK (engine.getActiveOutputFormat() == OutputFormat::Surround5_1);
    CHECK (engine.getActiveLayout().format == OutputFormat::Surround5_1);
    CHECK (engine.getActiveLayout().layout.numSpeakers > 0);

    engine.setOutputFormat (OutputFormat::AmbisonicsHOA);
    CHECK (engine.getActiveOutputFormat() == OutputFormat::AmbisonicsHOA);
    // Ambisonics output formats carry no speaker layout (channel-count only).
    CHECK (engine.getActiveLayout().layout.numSpeakers == 0);
}

// ----------------------------------------------------------------------------
// Escape hatches (D-02) — binaural renderer + active-renderer-index access
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: escape hatches expose the underlying BinauralRenderer instances",
           "[engine][escape-hatch]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    // Both double-buffered renderer slots must be reachable (consumer needs
    // this for SOFA profile loading -- CLAUDE.md gotcha: synchronous load
    // via getBinauralRenderer(index), not the timer thread, in test harnesses).
    CHECK_NOTHROW (engine.getBinauralRenderer (0));
    CHECK_NOTHROW (engine.getBinauralRenderer (1));

    int initialActive = engine.getActiveRendererIndexAtomic().load();
    int prepareIdx = engine.getPrepareRendererIndex();
    CHECK (prepareIdx != initialActive);

    engine.swapActiveRenderer();
    CHECK (engine.getActiveRendererIndexAtomic().load() == prepareIdx);
}

// ============================================================================
// SC-16 -- per-block layout snapshot, opt-in dispatch derivation and the
// three-slot layout handoff. Single-threaded on purpose: these pin the
// contract's semantics (which path renders, which layout wins), not the
// concurrency, which OpenSpatialPanner's [switchstress] test and ThreadSanitizer
// recipe cover. No std::thread here, so no thread-library link is needed.
// ============================================================================
namespace
{
    constexpr int kSc16MaxCh = MAX_SPEAKERS;

    // A Quad rear-left speaker (azimuth +135 deg, channel 2 in the Quad layout;
    // positive azimuth = left in SpatialCore). An object placed there lands
    // mostly on that speaker, so energy on channel 2 proves a surround render
    // against the Quad layout.
    constexpr float kQuadRearLeftAzimuthDeg = 135.0f;
    constexpr int kQuadRearLeftChannel = 2;

    struct Sc16Output
    {
        std::vector<std::vector<float>> storage = std::vector<std::vector<float>> (
            static_cast<size_t> (kSc16MaxCh), std::vector<float> (kBlockSize, 0.0f));
        float* ptrs[kSc16MaxCh] = {};

        Sc16Output()
        {
            for (int c = 0; c < kSc16MaxCh; ++c)
                ptrs[c] = storage[static_cast<size_t> (c)].data();
        }

        void clear()
        {
            for (auto& ch : storage)
                for (float& v : ch)
                    v = 0.0f;
        }

        // True when every channel from firstCh up is exactly zero.
        bool silentFrom (int firstCh) const
        {
            for (int c = firstCh; c < kSc16MaxCh; ++c)
                for (float v : storage[static_cast<size_t> (c)])
                    if (v != 0.0f)
                        return false;
            return true;
        }

        bool channelHasSignal (int ch) const
        {
            for (float v : storage[static_cast<size_t> (ch)])
                if (v != 0.0f)
                    return true;
            return false;
        }

        bool allChannelsFinite() const
        {
            for (const auto& ch : storage)
                for (float v : ch)
                    if (! std::isfinite (v))
                        return false;
            return true;
        }
    };

    // Renders two consecutive blocks so the gain ramp from zero has settled,
    // leaving the second block's output in `out`.
    void renderTwoBlocks (RenderEngine& engine, const RenderSources& sources,
                          const RenderBlockContext& ctx, Sc16Output& out)
    {
        for (int block = 0; block < 2; ++block)
        {
            out.clear();
            engine.renderBlock (sources, ctx, out.ptrs, kSc16MaxCh);
        }
    }

    RenderBlockContext makeSc16Context (bool derivesDispatch)
    {
        RenderBlockContext ctx;
        ctx.sampleRate = kSampleRate;
        ctx.engineComputesGains = true;   // surround and binaural paths need real gains
        ctx.engineDerivesDispatch = derivesDispatch;
        return ctx;
    }
}

TEST_CASE ("RenderEngine: engineDerivesDispatch renders the active layout's path even when the consumer's flags disagree",
           "[engine][format-switch][sc16]")
{
    SourceFixture fixture;

    SECTION ("Binaural layout, consumer claims discrete surround: only channels 0 and 1 carry signal")
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (OutputFormat::Binaural);

        RenderSources sources = fixture.makeSources();
        RenderBlockContext ctx = makeSc16Context (true);
        // Consumer's dispatch flags all false == "discrete surround", which
        // the Binaural layout (no speakers) cannot render.
        ctx.isBinaural = false;
        ctx.isStereoVariant = false;
        ctx.isAmbiOutput = false;

        Sc16Output out;
        renderTwoBlocks (engine, sources, ctx, out);

        CHECK (out.allChannelsFinite());
        CHECK (out.channelHasSignal (0));
        CHECK (out.channelHasSignal (1));
        CHECK (out.silentFrom (2));
    }

    SECTION ("Quad layout, consumer claims binaural: a rear-placed object reaches a Quad rear channel")
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (OutputFormat::Quad);

        RenderSources sources = fixture.makeSources();
        sources.objects[0].azimuthDeg = kQuadRearLeftAzimuthDeg;

        RenderBlockContext ctx = makeSc16Context (true);
        ctx.isBinaural = true;            // wrong for Quad; must be overwritten
        ctx.activeFormat = OutputFormat::Binaural;

        Sc16Output out;
        renderTwoBlocks (engine, sources, ctx, out);

        CHECK (out.allChannelsFinite());
        CHECK (out.channelHasSignal (kQuadRearLeftChannel));
    }
}

TEST_CASE ("RenderEngine: with engineDerivesDispatch false the consumer's dispatch flags are honoured verbatim",
           "[engine][format-switch][sc16]")
{
    SourceFixture fixture;

    SECTION ("Quad layout, consumer claims binaural: output lands only on channels 0 and 1")
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (OutputFormat::Quad);

        RenderSources sources = fixture.makeSources();
        sources.objects[0].azimuthDeg = kQuadRearLeftAzimuthDeg;

        RenderBlockContext ctx = makeSc16Context (false);
        ctx.isBinaural = true;
        ctx.activeFormat = OutputFormat::Binaural;

        Sc16Output out;
        renderTwoBlocks (engine, sources, ctx, out);

        CHECK (out.allChannelsFinite());
        CHECK (out.channelHasSignal (0));
        CHECK (out.silentFrom (2));
        CHECK_FALSE (out.channelHasSignal (kQuadRearLeftChannel));
    }

    SECTION ("Binaural layout, consumer claims discrete surround: the Binaural layout has no speakers, so nothing renders")
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (OutputFormat::Binaural);

        RenderSources sources = fixture.makeSources();
        RenderBlockContext ctx = makeSc16Context (false);
        ctx.isBinaural = false;
        ctx.isStereoVariant = false;
        ctx.isAmbiOutput = false;

        Sc16Output out;
        renderTwoBlocks (engine, sources, ctx, out);

        CHECK (out.allChannelsFinite());
        CHECK (out.silentFrom (0));
    }
}

TEST_CASE ("RenderEngine: several setOutputFormat calls with no block between them render the last one",
           "[engine][format-switch][sc16]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    sources.objects[0].azimuthDeg = kQuadRearLeftAzimuthDeg;

    // The consumer's flags are deliberately left at their defaults: the
    // engine must derive the dispatch from whichever layout it acquires.
    const RenderBlockContext ctx = makeSc16Context (true);

    // Back-to-back switches with no block rendered in between (the sequence
    // that used to rewrite the slot being rendered).
    engine.setOutputFormat (OutputFormat::Quad);
    engine.setOutputFormat (OutputFormat::Binaural);
    engine.setOutputFormat (OutputFormat::Surround5_1);
    engine.setOutputFormat (OutputFormat::Binaural);

    CHECK (engine.getActiveOutputFormat() == OutputFormat::Binaural);
    CHECK (engine.getActiveLayout().format == OutputFormat::Binaural);

    Sc16Output out;
    renderTwoBlocks (engine, sources, ctx, out);

    CHECK (out.allChannelsFinite());
    CHECK (out.channelHasSignal (0));
    CHECK (out.channelHasSignal (1));
    CHECK (out.silentFrom (2));

    // And again from a rendered state: Binaural -> Quad is picked up by the
    // next block, which carries energy on a Quad rear channel.
    engine.setOutputFormat (OutputFormat::Quad);
    CHECK (engine.getActiveOutputFormat() == OutputFormat::Quad);

    renderTwoBlocks (engine, sources, ctx, out);

    CHECK (out.allChannelsFinite());
    CHECK (out.channelHasSignal (kQuadRearLeftChannel));
}

TEST_CASE ("RenderEngine: a block rendered before any setOutputFormat uses the default binaural layout",
           "[engine][format-switch][sc16]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    // No setOutputFormat() call at all.
    CHECK (engine.getActiveOutputFormat() == OutputFormat::Binaural);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    const RenderBlockContext ctx = makeSc16Context (true);

    Sc16Output out;
    renderTwoBlocks (engine, sources, ctx, out);

    CHECK (out.allChannelsFinite());
    CHECK (out.channelHasSignal (0));
    CHECK (out.channelHasSignal (1));
    CHECK (out.silentFrom (2));
}

// ----------------------------------------------------------------------------
// D-06(a) extended by D-19(ii): RenderEngine holds the last finite azimuth,
// elevation and distance per object slot, field by field, and substitutes it for
// any non-finite value before every render path and before engine-side gain
// computation. Each test builds two engines identically and varies ONLY the
// position fields, then compares full output buffers with exact ==.
// ----------------------------------------------------------------------------
namespace
{
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();

    enum class SanitizePath { SurroundEngineGains, AmbisonicsHOA, BinauralHRTF };

    /** One engine + fixture + context + output storage for one render path. */
    struct SanitizeRig
    {
        RenderEngine engine;
        SourceFixture fixture;
        RenderBlockContext ctx;
        int numCh = 0;
        std::vector<std::vector<float>> out;
        std::vector<float*> ptrs;

        SanitizeRig (SanitizePath path, OutputFormat format)
        {
            engine.prepare (kSampleRate, kBlockSize);
            engine.setOutputFormat (format);
            ctx.sampleRate = kSampleRate;
            ctx.activeFormat = format;

            switch (path)
            {
                case SanitizePath::SurroundEngineGains:
                    ctx.engineComputesGains = true;
                    numCh = engine.getActiveLayout().layout.totalChannels;
                    break;
                case SanitizePath::AmbisonicsHOA:
                    ctx.isAmbiOutput = true;
                    ctx.ambiOrder = 3;
                    numCh = 16;
                    break;
                case SanitizePath::BinauralHRTF:
                    ctx.isBinaural = true;
                    ctx.useHRTF = true;
                    numCh = 2;
                    break;
            }

            out.assign (static_cast<size_t> (numCh), std::vector<float> (kBlockSize, 0.0f));
            for (auto& ch : out)
                ptrs.push_back (ch.data());
        }

        void render (float azDeg, float elDeg, float dist, bool live = true)
        {
            for (auto& ch : out)
                std::fill (ch.begin(), ch.end(), 0.0f);

            RenderSources s = fixture.makeSources (live);
            s.objects[0].azimuthDeg   = azDeg;
            s.objects[0].elevationDeg = elDeg;
            s.objects[0].distance     = dist;
            engine.renderBlock (s, ctx, ptrs.data(), numCh);
        }

        bool outputFinite() const
        {
            for (const auto& ch : out)
                if (! allFinite (ch.data(), kBlockSize))
                    return false;
            return true;
        }
    };

    /** Exact == on every sample of every channel; reports the first mismatch. */
    void requireIdenticalOutput (const SanitizeRig& a, const SanitizeRig& b)
    {
        REQUIRE (a.numCh == b.numCh);
        for (int c = 0; c < a.numCh; ++c)
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float va = a.out[static_cast<size_t> (c)][static_cast<size_t> (i)];
                const float vb = b.out[static_cast<size_t> (c)][static_cast<size_t> (i)];
                if (! (va == vb))
                {
                    INFO ("channel " << c << " sample " << i << " got " << va << " expected " << vb);
                    REQUIRE (va == vb);
                }
            }
    }

    bool anyOutputNonzero (const SanitizeRig& r)
    {
        for (const auto& ch : r.out)
            if (anyNonzero (ch.data(), kBlockSize))
                return true;
        return false;
    }
}

TEST_CASE ("RenderEngine: a NaN azimuth block renders exactly like repeating the last finite azimuth (D-06a)",
           "[engine][sanitize]")
{
    auto a = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Quad);
    auto b = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Quad);
    REQUIRE (a->numCh >= 4);

    a->render (30.0f, 0.0f, 0.5f);
    b->render (30.0f, 0.0f, 0.5f);
    requireIdenticalOutput (*a, *b);

    a->render (kNaN, 0.0f, 0.5f);
    b->render (30.0f, 0.0f, 0.5f);
    CHECK (a->outputFinite());
    CHECK (anyOutputNonzero (*a));
    requireIdenticalOutput (*a, *b);
}

TEST_CASE ("RenderEngine: a first-ever non-finite position renders exactly like the ObjectState defaults 0, 0, 0.5 (D-06a, D-19ii)",
           "[engine][sanitize]")
{
    auto a = std::make_unique<SanitizeRig> (SanitizePath::AmbisonicsHOA, OutputFormat::AmbisonicsHOA);
    auto b = std::make_unique<SanitizeRig> (SanitizePath::AmbisonicsHOA, OutputFormat::AmbisonicsHOA);

    for (int block = 0; block < 2; ++block)
    {
        INFO ("block " << block);
        a->render (kInf, kNaN, kNaN);
        b->render (0.0f, 0.0f, 0.5f);
        CHECK (a->outputFinite());
        requireIdenticalOutput (*a, *b);
    }
}

TEST_CASE ("RenderEngine: a NaN distance never reaches the NFC-HOA filter state; output matches the last good distance (D-19ii, F7)",
           "[engine][sanitize]")
{
    auto a = std::make_unique<SanitizeRig> (SanitizePath::AmbisonicsHOA, OutputFormat::AmbisonicsHOA);
    auto b = std::make_unique<SanitizeRig> (SanitizePath::AmbisonicsHOA, OutputFormat::AmbisonicsHOA);

    a->render (30.0f, 10.0f, 0.4f);
    b->render (30.0f, 10.0f, 0.4f);
    CHECK (a->outputFinite());

    for (int block = 2; block <= 4; ++block)
    {
        INFO ("block " << block);
        a->render (30.0f, 10.0f, kNaN);
        b->render (30.0f, 10.0f, 0.4f);
        CHECK (a->outputFinite());
    }

    // Block 4 equals an engine fed 0.4 throughout.
    requireIdenticalOutput (*a, *b);
}

TEST_CASE ("RenderEngine: positions are held per field, so a new finite azimuth with a NaN elevation keeps the new azimuth (D-06a, F7)",
           "[engine][sanitize]")
{
    // 7.1.4 + engine-computed gains: the 3D VBAP path, where elevation matters.
    auto a = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Surround7_1_4);
    auto b = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Surround7_1_4);

    a->render (30.0f, 20.0f, 0.5f);
    b->render (30.0f, 20.0f, 0.5f);

    // ADM-OSC forwards NaN for "field not set" on a single-axis /azim update.
    a->render (60.0f, kNaN, 0.5f);
    b->render (60.0f, 20.0f, 0.5f);
    CHECK (a->outputFinite());
    CHECK (anyOutputNonzero (*a));
    requireIdenticalOutput (*a, *b);
}

TEST_CASE ("RenderEngine: held positions belong to the slot, keep updating while it is not live, and carry over to a reused slot (D-06a, IN-04)",
           "[engine][sanitize]")
{
    // Pins the "per object slot" wording in docs/integration-guide.md. Three
    // engines share the same history except for the last block.
    auto a = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Quad);
    auto b = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Quad);
    auto c = std::make_unique<SanitizeRig> (SanitizePath::SurroundEngineGains, OutputFormat::Quad);

    for (auto* r : { a.get(), b.get(), c.get() })
    {
        r->render (30.0f, 0.0f, 0.5f);           // first occupant, live
        r->render (90.0f, 0.0f, 0.5f, false);    // slot not live: 90 still becomes the held azimuth
    }

    // The slot is reused and its first update leaves the azimuth unset.
    a->render (kNaN, 0.0f, 0.5f);
    b->render (90.0f, 0.0f, 0.5f);   // the slot's last finite azimuth
    c->render (0.0f, 0.0f, 0.5f);    // the ObjectState default
    CHECK (a->outputFinite());
    CHECK (anyOutputNonzero (*a));
    requireIdenticalOutput (*a, *b);

    // And that is distinguishable from the default position.
    bool differsFromDefault = false;
    for (int ch = 0; ch < a->numCh; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            if (a->out[static_cast<size_t> (ch)][static_cast<size_t> (i)] != c->out[static_cast<size_t> (ch)][static_cast<size_t> (i)])
                differsFromDefault = true;
    CHECK (differsFromDefault);
}

TEST_CASE ("RenderEngine: the direct-binaural HRTF branch stays finite for an infinite elevation (D-06a)",
           "[engine][sanitize]")
{
    auto a = std::make_unique<SanitizeRig> (SanitizePath::BinauralHRTF, OutputFormat::Binaural);

    // Load a real SOFA profile into the active renderer so the elevation
    // actually reaches the HRIR lookup (in Simple mode updateSourceHRIR
    // returns before reading the direction).
    const int active = a->engine.getActiveRendererIndexAtomic().load();
    auto& renderer = a->engine.getBinauralRenderer (active);
    REQUIRE (renderer.hrtfDatabase.loadFromFile (test::getSofaFile ("sadie_d2_ku100.sofa"),
                                                  static_cast<float> (kSampleRate)));
    renderer.setProfile (1);
    REQUIRE_FALSE (renderer.isSimpleMode());

    for (int block = 0; block < 3; ++block)
    {
        INFO ("block " << block);
        a->render (30.0f, kInf, 0.5f);
        CHECK (a->outputFinite());
    }
    CHECK (anyOutputNonzero (*a));
}

// ----------------------------------------------------------------------------
// D-09: RenderEngine has no private SH decoder. activateLayout fills
// ambiDecodeMatrix through AmbisonicsCodec::getDecodeMatrix at order 3. The
// pin below compares it with the pre-change private decoder, copied from the
// git blob d43cb15:src/Engine/RenderEngine.cpp lines 650-734 so it is the
// pre-change code regardless of when this test runs. Only the name and the
// static-member qualification differ; the body is byte-identical (left
// unindented so it diffs cleanly against the blob). It uses ACN 0-15, which the
// D-08 constant fix does not touch, so the pin is unaffected by that fix. The
// comparison is within float rounding, not exact, because the two copies are
// compiled separately; see kAmbiPinTolerance below.
// ----------------------------------------------------------------------------
// Indentation in this namespace is split on purpose (IN-09):
// referenceAmbiDecode is left unindented so it diffs byte-for-byte against the
// d43cb15 blob; the helpers after it are indented as everywhere else in this
// file. Do not re-indent referenceAmbiDecode.
namespace
{
void referenceAmbiDecode (const SpeakerLayout& layout,
                          float (*outMatrix)[MAX_SPEAKERS],
                          int& outNumSpeakers)
{
    const int N = layout.numSpeakers;
    const int M = 16; // HOA_CHANNELS (surround decode cap, 3rd order)
    outNumSpeakers = N;

    // Build encoding matrix E[c][s] = evalSH(c, speaker_s_position)
    float E[16][16] = {};
    for (int s = 0; s < N; ++s)
        for (int c = 0; c < M; ++c)
            E[c][s] = evalSH (c, layout.speakers[s].azimuthRad,
                                 layout.speakers[s].elevationRad);

    // Compute EET = E * E^T  (M x M)
    float EET[16][16] = {};
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j)
        {
            float sum = 0.0f;
            for (int s = 0; s < N; ++s)
                sum += E[i][s] * E[j][s];
            EET[i][j] = sum;
        }

    // Tikhonov regularization: EET += epsilon * I
    float epsilon = 0.01f;
    for (int i = 0; i < M; ++i)
        EET[i][i] += epsilon;

    // Invert EET via Gauss-Jordan (M x M, small matrix)
    float inv[16][16] = {};
    for (int i = 0; i < M; ++i)
        inv[i][i] = 1.0f;

    float aug[16][16];
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < M; ++j)
            aug[i][j] = EET[i][j];

    for (int col = 0; col < M; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < M; ++row)
            if (std::abs (aug[row][col]) > std::abs (aug[pivot][col]))
                pivot = row;

        if (pivot != col)
        {
            std::swap_ranges (aug[col], aug[col] + M, aug[pivot]);
            std::swap_ranges (inv[col], inv[col] + M, inv[pivot]);
        }

        float diagVal = aug[col][col];
        if (std::abs (diagVal) < 1e-10f) continue;

        for (int j = 0; j < M; ++j)
        {
            aug[col][j] /= diagVal;
            inv[col][j] /= diagVal;
        }

        for (int row = 0; row < M; ++row)
        {
            if (row == col) continue;
            float factor = aug[row][col];
            for (int j = 0; j < M; ++j)
            {
                aug[row][j] -= factor * aug[col][j];
                inv[row][j] -= factor * inv[col][j];
            }
        }
    }

    // D[s][c] = sum_k E^T[s][k] * inv[k][c] = sum_k E[k][s] * inv[k][c]
    for (int s = 0; s < N; ++s)
        for (int c = 0; c < M; ++c)
        {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k)
                sum += E[k][s] * inv[k][c];
            outMatrix[s][c] = sum;
        }
}

    // The 15 OutputFormats that resolve to a speaker layout.
    constexpr OutputFormat kSpeakerFormats[] = {
        OutputFormat::Quad,          OutputFormat::Surround5_0,   OutputFormat::Surround5_1,
        OutputFormat::Surround7_0,   OutputFormat::Surround7_1,   OutputFormat::Surround9_1,
        OutputFormat::Octaphonic,    OutputFormat::Surround5_1_2, OutputFormat::Surround5_1_4,
        OutputFormat::Surround7_1_2, OutputFormat::Surround7_1_4, OutputFormat::Surround7_1_6,
        OutputFormat::Surround9_1_4, OutputFormat::Surround9_1_6, OutputFormat::SurroundSML13_1
    };
    static_assert (sizeof (kSpeakerFormats) / sizeof (kSpeakerFormats[0]) == NUM_LAYOUT_DEFS,
                   "one speaker OutputFormat per LayoutID");

    // Derivation of the two bounds below (G-02-10; full evidence in
    // .planning/debug/ambi-pin-release-tolerance.md).
    //
    // The library decoder and referenceAmbiDecode run the same float algorithm
    // in different translation units. At -O3 on FMA hardware (Apple Silicon)
    // clang contracts the E*E^T sums into fused multiply-adds, and the
    // vectoriser keeps only the scalar tail fused, so the two copies round
    // differently. Debug (-O0) vectorises nothing and is bit-identical; Release
    // arm64 differs by up to 5.3e-6 on 10 of 15 layouts.
    //
    // E*E^T is singular (rank at most 15 of 16) and only the 0.01 Tikhonov term
    // makes it invertible, with cond(EET + 0.01 I) from 501 (Quad) to 1786
    // (9.1.6). One rounding therefore moves the decode by 1e-6 to 1e-5.
    //
    // Measured: the spread between the two float decoders is 0 in Debug, 5.3e-6
    // in Release arm64, and 1.0e-5 for the worst compiler variant tried. Each
    // float decoder sits up to 1.4e-5 (Release) / 1.7e-5 (Debug) / 1.8e-5 (any
    // variant tried) from a double-precision solve of the same E.
    //
    // The smallest change the pin must catch is a +0.1% Tikhonov epsilon. It
    // moves the decode by 5.8e-5 to 6.3e-5 on 7.0, 7.1, 7.1.2, 7.1.4 and 7.1.6,
    // while real regressions (wrong order, stride, speaker order, dropped
    // regularisation) move it by 0.1 to 1. A +0.01% change (about 6e-6) is below
    // rounding, and no tolerance can resolve it. The +0.1% resolution holds on
    // those five 7.x layouts only: on the other ten the same change moves the
    // decode by 9.6e-7 (Quad) to 2.2e-5 (5.1.2), under the bound, so a
    // regression confined to them is caught only if it is of the 0.1 to 1
    // kind. The pin as a whole still fails a +0.1% change, through the 7.x
    // layouts (IN-07; measured with a double solve of each layout's E).
    //
    // Hence kAmbiPinTolerance = 2.5e-5: about 2.5x above the worst rounding
    // spread (1.0e-5) and 2.3x below the +0.1% change (5.8e-5). kAmbiFloatVsDoubleTolerance =
    // 4e-5 is about 2.2x the worst float-vs-double distance. It re-checks the
    // premise above on every build, so a toolchain that moves the noise floor
    // fails there with a message that says so, instead of looking like a
    // library change.
    //
    // The previous bound (1e-6) came from RESEARCH F10, a Debug-grade "worst 0"
    // measurement plus an unmeasured margin.
    constexpr float kAmbiPinTolerance = 2.5e-5f;
    constexpr double kAmbiFloatVsDoubleTolerance = 4.0e-5;

    // Non-finite-aware max-reduction (WR-06), shared with the other test files
    // through tests/TestNumerics.h (WR-07).
    using spatialcore_test::accumulateWorstFinite;

    // The same decode as referenceAmbiDecode, in double, on the same float
    // inputs: E is bit-identical to what both float decoders see, and the
    // Tikhonov term is the float epsilon promoted, so only precision differs.
    // Fixed-size stack arrays, no heap. The caller zero-initialises outMatrix.
    void doublePrecisionAmbiDecode (const SpeakerLayout& layout,
                                    double (*outMatrix)[MAX_SPEAKERS])
    {
        constexpr int M = 16;
        const int N = layout.numSpeakers;

        // IN-08: E is M x M, indexed E[c][s] for s < N, so more speakers than
        // order-3 channels would write past it.
        REQUIRE (N >= 0);
        REQUIRE (N <= M);

        double E[M][M] = {};
        for (int s = 0; s < N; ++s)
            for (int c = 0; c < M; ++c)
                E[c][s] = static_cast<double> (evalSH (c, layout.speakers[s].azimuthRad,
                                                          layout.speakers[s].elevationRad));

        double aug[M][M] = {};
        double inv[M][M] = {};
        for (int i = 0; i < M; ++i)
        {
            for (int j = 0; j < M; ++j)
            {
                double sum = 0.0;
                for (int s = 0; s < N; ++s)
                    sum += E[i][s] * E[j][s];
                aug[i][j] = sum;
            }
            aug[i][i] += static_cast<double> (0.01f);
            inv[i][i] = 1.0;
        }

        // Gauss-Jordan with partial pivoting.
        for (int col = 0; col < M; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < M; ++row)
                if (std::abs (aug[row][col]) > std::abs (aug[pivot][col]))
                    pivot = row;
            if (pivot != col)
                for (int j = 0; j < M; ++j)
                {
                    std::swap (aug[col][j], aug[pivot][j]);
                    std::swap (inv[col][j], inv[pivot][j]);
                }

            // IN-08: the library skips a pivot below 1e-10; this exact
            // reference must never meet one (the 0.01 Tikhonov term keeps
            // every pivot near 0.01 or above), so it stops by name instead of
            // dividing into NaN.
            const double diag = aug[col][col];
            REQUIRE (std::abs (diag) >= 1e-10);
            for (int j = 0; j < M; ++j)
            {
                aug[col][j] /= diag;
                inv[col][j] /= diag;
            }
            for (int row = 0; row < M; ++row)
            {
                if (row == col) continue;
                const double f = aug[row][col];
                for (int j = 0; j < M; ++j)
                {
                    aug[row][j] -= f * aug[col][j];
                    inv[row][j] -= f * inv[col][j];
                }
            }
        }

        for (int s = 0; s < N; ++s)
            for (int c = 0; c < M; ++c)
            {
                double sum = 0.0;
                for (int k = 0; k < M; ++k)
                    sum += E[k][s] * inv[k][c];
                outMatrix[s][c] = sum;
            }
    }
}

TEST_CASE ("RenderEngine: the order-3 speaker decode matches the pre-change private decoder within float rounding on all 15 layouts (D-09)",
           "[engine][ambi-pin]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    float worst = 0.0f;
    int layoutsChecked = 0;
    for (const auto format : kSpeakerFormats)
    {
        engine.setOutputFormat (format);
        const auto& active = engine.getActiveLayout();
        REQUIRE (active.format == format);

        float reference[MAX_SPEAKERS][MAX_SPEAKERS] = {};
        int refNumSpeakers = -1;
        referenceAmbiDecode (active.layout, reference, refNumSpeakers);

        INFO ("format " << static_cast<int> (format) << " (" << OutputFormatRegistry::getDisplayName (format) << ")");
        CHECK (active.layout.numSpeakers > 0);
        CHECK (refNumSpeakers == active.layout.numSpeakers);
        CHECK (active.ambiNumSpeakers == active.layout.numSpeakers);

        double exact[MAX_SPEAKERS][MAX_SPEAKERS] = {};
        doublePrecisionAmbiDecode (active.layout, exact);

        float worstHere = 0.0f;
        double libVsExact = 0.0;
        double refVsExact = 0.0;
        int nonFinite = 0;
        for (int s = 0; s < active.layout.numSpeakers; ++s)
            for (int c = 0; c < 16; ++c)
            {
                // accumulateWorstFinite rejects a NaN or Inf distance instead of
                // letting std::max drop it (WR-06). Count the rejects and fail
                // once per layout (IN-10).
                const float  dNew = std::abs (active.ambiDecodeMatrix[s][c] - reference[s][c]);
                const double dLib = std::abs (static_cast<double> (active.ambiDecodeMatrix[s][c]) - exact[s][c]);
                const double dRef = std::abs (static_cast<double> (reference[s][c]) - exact[s][c]);
                const bool newOk = accumulateWorstFinite (worstHere, dNew);
                const bool libOk = accumulateWorstFinite (libVsExact, dLib);
                const bool refOk = accumulateWorstFinite (refVsExact, dRef);
                // Name only the first bad entry; the count says how many.
                if (! (newOk && libOk && refOk) && nonFinite++ == 0)
                    UNSCOPED_INFO ("first non-finite decode entry: s=" << s << " c=" << c
                                   << " lib=" << active.ambiDecodeMatrix[s][c]
                                   << " ref=" << reference[s][c]
                                   << " exact=" << exact[s][c]);
            }
        CHECK (nonFinite == 0);
        INFO ("worst |new - old| = " << worstHere);
        INFO ("worst |library - double solve| = " << libVsExact);
        INFO ("worst |reference - double solve| = " << refVsExact);
        CHECK (worstHere <= kAmbiPinTolerance);
        CHECK (libVsExact <= kAmbiFloatVsDoubleTolerance);
        CHECK (refVsExact <= kAmbiFloatVsDoubleTolerance);
        worst = std::max (worst, worstHere);
        ++layoutsChecked;
    }
    CHECK (layoutsChecked == 15);
    INFO ("worst |new - old| over all 15 layouts = " << worst);
    CHECK (worst <= kAmbiPinTolerance);
}

TEST_CASE ("RenderEngine: the ambi-pin max-reduction rejects non-finite distances instead of dropping them (WR-06, IN-11)",
           "[engine][ambi-pin]")
{
    const double quietNaN = std::numeric_limits<double>::quiet_NaN();
    const double posInf = std::numeric_limits<double>::infinity();

    double worst = 0.25;
    CHECK_FALSE (accumulateWorstFinite (worst, quietNaN));
    CHECK_FALSE (accumulateWorstFinite (worst, posInf));
    CHECK_FALSE (accumulateWorstFinite (worst, -posInf));
    CHECK (worst == 0.25);   // a rejected distance leaves the maximum untouched

    // A rejected distance must not poison later finite ones either.
    CHECK (accumulateWorstFinite (worst, 0.5));
    CHECK (worst == 0.5);
    CHECK (accumulateWorstFinite (worst, 0.1));
    CHECK (worst == 0.5);

    // A NaN on an untouched maximum: bare std::max would keep 0.0 and report no
    // problem, which is exactly the WR-06 vacuous pass.
    double fresh = 0.0;
    CHECK_FALSE (accumulateWorstFinite (fresh, quietNaN));
    CHECK (accumulateWorstFinite (fresh, 0.125));
    CHECK (fresh == 0.125);

    // The float instantiation, as used for worstHere.
    float worstF = 0.0f;
    CHECK_FALSE (accumulateWorstFinite (worstF, std::numeric_limits<float>::quiet_NaN()));
    CHECK_FALSE (accumulateWorstFinite (worstF, std::numeric_limits<float>::infinity()));
    CHECK (worstF == 0.0f);
    CHECK (accumulateWorstFinite (worstF, 2.0f));
    CHECK (worstF == 2.0f);
}

TEST_CASE ("RenderEngine: decode rows beyond the speaker count are cleared on a layout switch (D-09)",
           "[engine][ambi-pin]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    // Double-buffered: 9.1.6 fills one buffer, Binaural takes the other, and
    // Quad lands back in 9.1.6's buffer. Rows 4-15 must not keep 9.1.6 data.
    engine.setOutputFormat (OutputFormat::Surround9_1_6);
    REQUIRE (engine.getActiveLayout().layout.numSpeakers == 15);
    engine.setOutputFormat (OutputFormat::Binaural);
    engine.setOutputFormat (OutputFormat::Quad);

    const auto& active = engine.getActiveLayout();
    REQUIRE (active.layout.numSpeakers == 4);
    int staleEntries = 0;
    for (int s = active.layout.numSpeakers; s < MAX_SPEAKERS; ++s)
        for (int c = 0; c < MAX_SPEAKERS; ++c)
            if (active.ambiDecodeMatrix[s][c] != 0.0f)
                ++staleEntries;
    CHECK (staleEntries == 0);
}

// ----------------------------------------------------------------------------
// Criterion 3: every OutputFormat resolves through RenderEngine::setOutputFormat
// to a layout that agrees with OutputFormatRegistry (RESEARCH Reference Data D).
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: all 23 output formats resolve to layouts that agree with the registry (criterion 3)",
           "[engine][format-resolve]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    int formatsChecked = 0, speakerFormats = 0;
    for (int i = 0; i < NUM_OUTPUT_FORMATS; ++i)
    {
        const auto format = static_cast<OutputFormat> (i);
        const auto& info = OutputFormatRegistry::getInfo (format);
        REQUIRE (info.format == format);

        engine.setOutputFormat (format);
        const auto& active = engine.getActiveLayout();
        const auto& layout = active.layout;

        INFO ("format " << i << " (" << info.name << ")");
        CHECK (active.format == format);
        CHECK (layout.totalChannels == info.requiredChannels);
        CHECK ((layout.lfeChannelIndex >= 0) == info.hasLFE);

        const bool isSpeakerFormat = ! info.isAmbisonicsOutput && ! info.isStereoVariant
                                     && format != OutputFormat::Binaural;
        if (isSpeakerFormat)
        {
            ++speakerFormats;
            CHECK (layoutHasHeight (layout) == info.hasHeight);
            CHECK (layout.numSpeakers > 0);

            bool seen[MAX_SPEAKERS] = {};
            for (int s = 0; s < layout.numSpeakers; ++s)
            {
                const int ch = layout.speakers[s].channelIndex;
                INFO ("speaker " << s << " -> channel " << ch);
                CHECK (ch >= 0);
                CHECK (ch < layout.totalChannels);
                CHECK (ch != layout.lfeChannelIndex);
                if (ch >= 0 && ch < MAX_SPEAKERS)
                {
                    CHECK_FALSE (seen[ch]);   // no two speakers share a channel
                    seen[ch] = true;
                }
            }

            if (info.hasHeight)
            {
                int regular = 0;
                for (const auto& t : active.vbapTriplets)
                    if (! t.lowerHemisphere)
                        ++regular;
                CHECK (regular > 0);
            }
            else
            {
                CHECK (active.vbapTriplets.empty());
            }
        }
        else
        {
            CHECK (layout.numSpeakers == 0);
            CHECK (active.vbapTriplets.empty());
        }
        ++formatsChecked;
    }
    CHECK (formatsChecked == 23);
    CHECK (speakerFormats == 15);
}

// ----------------------------------------------------------------------------
// SC-18 part 1 — runtime speaker-algorithm selection through the engine
// ----------------------------------------------------------------------------
namespace
{
    constexpr int kSc18Channels = 12; // 7.1.4

    using Sc18Render = std::vector<std::vector<float>>;

    // Renders one steady DC source on a 7.1.4 engine that computes its own gains.
    // algorithmIndex < 0 leaves the engine on its default (never calls the setter).
    // Several warm-up blocks let gain interpolation settle; the last block is returned.
    Sc18Render sc18Render (int algorithmIndex, float azimuthDeg, float elevationDeg)
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (OutputFormat::Surround7_1_4);
        if (algorithmIndex >= 0)
            engine.setAlgorithmIndex (algorithmIndex);

        SourceFixture fixture;
        RenderSources sources = fixture.makeSources();
        sources.objects[0].azimuthDeg = azimuthDeg;
        sources.objects[0].elevationDeg = elevationDeg;

        RenderBlockContext ctx;
        ctx.sampleRate = kSampleRate;
        ctx.engineComputesGains = true;
        ctx.engineDerivesDispatch = true;

        Sc18Render out (kSc18Channels, std::vector<float> (kBlockSize, 0.0f));
        float* outPtrs[kSc18Channels] = {};
        for (int c = 0; c < kSc18Channels; ++c)
            outPtrs[c] = out[static_cast<size_t> (c)].data();

        for (int block = 0; block < 32; ++block)
        {
            for (auto& ch : out)
                std::fill (ch.begin(), ch.end(), 0.0f);
            engine.renderBlock (sources, ctx, outPtrs, kSc18Channels);
        }
        return out;
    }

    float sc18MaxDiff (const Sc18Render& a, const Sc18Render& b)
    {
        float worst = 0.0f;
        for (size_t c = 0; c < a.size(); ++c)
            for (size_t i = 0; i < a[c].size(); ++i)
                worst = std::max (worst, std::abs (a[c][i] - b[c][i]));
        return worst;
    }

    bool sc18AllFinite (const Sc18Render& r)
    {
        for (const auto& ch : r)
            if (! allFinite (ch.data(), static_cast<int> (ch.size())))
                return false;
        return true;
    }
}

TEST_CASE ("RenderEngine: a fresh engine defaults to VBAP and an explicit VBAP selection is sample-identical (SC-18)",
           "[engine][sc18]")
{
    RenderEngine fresh;
    CHECK (fresh.getAlgorithmIndex() == kAlgorithmIndexVBAP);

    const auto byDefault = sc18Render (-1, 45.0f, 0.0f);
    const auto explicitVbap = sc18Render (kAlgorithmIndexVBAP, 45.0f, 0.0f);
    CHECK (sc18MaxDiff (byDefault, explicitVbap) == 0.0f);
    CHECK (sc18AllFinite (byDefault));
}

TEST_CASE ("RenderEngine: the seven speaker algorithms render pairwise-distinct 7.1.4 output (SC-18)",
           "[engine][sc18]")
{
    // First of these positions at which all 21 pairs differ by more than 1e-3.
    const float candidates[][2] = { { 45.0f, 20.0f }, { 60.0f, 10.0f }, { 100.0f, 30.0f } };

    bool found = false;
    for (const auto& pos : candidates)
    {
        std::vector<Sc18Render> renders;
        for (int idx = 0; idx < kNumSpeakerAlgorithmIndices; ++idx)
        {
            renders.push_back (sc18Render (idx, pos[0], pos[1]));
            REQUIRE (sc18AllFinite (renders.back()));
        }

        bool allDiffer = true;
        for (int a = 0; a < kNumSpeakerAlgorithmIndices && allDiffer; ++a)
            for (int b = a + 1; b < kNumSpeakerAlgorithmIndices; ++b)
                if (sc18MaxDiff (renders[static_cast<size_t> (a)], renders[static_cast<size_t> (b)]) <= 1.0e-3f)
                {
                    INFO ("pair " << a << "," << b << " at azimuth " << pos[0] << " elevation " << pos[1]);
                    allDiffer = false;
                    break;
                }

        if (allDiffer)
        {
            INFO ("distinctness position: azimuth " << pos[0] << " elevation " << pos[1]);
            found = true;
            break;
        }
    }
    CHECK (found);
}

TEST_CASE ("RenderEngine: the algorithm index clamps, and stereo indices on a speaker layout render as VBAP (SC-18)",
           "[engine][sc18]")
{
    RenderEngine engine;
    engine.setAlgorithmIndex (-5);
    CHECK (engine.getAlgorithmIndex() == 0);
    engine.setAlgorithmIndex (99);
    CHECK (engine.getAlgorithmIndex() == kNumAlgorithmIndices - 1);
    CHECK (kNumAlgorithmIndices - 1 == 11);

    const auto vbap = sc18Render (kAlgorithmIndexVBAP, 45.0f, 20.0f);
    for (int idx = kNumSpeakerAlgorithmIndices; idx < kNumAlgorithmIndices; ++idx)
    {
        const auto r = sc18Render (idx, 45.0f, 20.0f);
        CHECK (sc18AllFinite (r));
        CHECK (sc18MaxDiff (r, vbap) == 0.0f);
    }

    // Out-of-range input behaves as the clamped extremes, still finite.
    CHECK (sc18AllFinite (sc18Render (-5, 45.0f, 20.0f)));
    CHECK (sc18AllFinite (sc18Render (99, 45.0f, 20.0f)));
}

// ----------------------------------------------------------------------------
// SC-18 part 2 — engine-owned stereo-mode gains for the Stereo format (D-06)
// ----------------------------------------------------------------------------
namespace
{
    struct Sc18StereoRender
    {
        std::vector<float> left;
        std::vector<float> right;
    };

    float sc18Rms (const std::vector<float>& v)
    {
        double sum = 0.0;
        for (float x : v)
            sum += static_cast<double> (x) * x;
        return static_cast<float> (std::sqrt (sum / static_cast<double> (v.size())));
    }

    // One steady DC source (0.5) on the Stereo format with engineComputesGains and
    // engineDerivesDispatch set. `distanceGain` is the constant block distance gain
    // the consumer hands in. Several warm-up blocks let the gain interpolation
    // settle; the last block is returned.
    Sc18StereoRender sc18StereoRender (int algorithmIndex, float azimuthDeg, float distanceGain = 1.0f)
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (OutputFormat::Stereo);
        engine.setAlgorithmIndex (algorithmIndex);

        SourceFixture fixture;
        std::fill (fixture.distGain.begin(), fixture.distGain.end(), distanceGain);
        RenderSources sources = fixture.makeSources();
        sources.objects[0].azimuthDeg = azimuthDeg;
        sources.objects[0].elevationDeg = 0.0f;

        RenderBlockContext ctx;
        ctx.sampleRate = kSampleRate;
        ctx.engineComputesGains = true;
        ctx.engineDerivesDispatch = true;

        Sc18StereoRender out { std::vector<float> (kBlockSize, 0.0f), std::vector<float> (kBlockSize, 0.0f) };
        float* outPtrs[2] = { out.left.data(), out.right.data() };
        for (int block = 0; block < 32; ++block)
        {
            std::fill (out.left.begin(), out.left.end(), 0.0f);
            std::fill (out.right.begin(), out.right.end(), 0.0f);
            engine.renderBlock (sources, ctx, outPtrs, 2);
        }
        return out;
    }
}

TEST_CASE ("RenderEngine: the five stereo modes give five different L/R pairs when the engine computes the gains (SC-18 part 2)",
           "[engine][sc18]")
{
    // Azimuth 20, not 45: at +45 degrees Stereo VBAP (speakers at +/-30, clamped) and
    // Blumlein (+/-45, exactly on the boundary) both give (L, R) = (1, 0) and are
    // indistinguishable. At 20 degrees all five modes land on different pairs.
    constexpr float kAzimuth = 20.0f;
    struct Pair { float l, r; };
    std::vector<Pair> pairs;
    for (int idx = kAlgorithmIndexEqualPower; idx <= kAlgorithmIndexBlumlein; ++idx)
    {
        const auto r = sc18StereoRender (idx, kAzimuth);
        REQUIRE (allFinite (r.left.data(), kBlockSize));
        REQUIRE (allFinite (r.right.data(), kBlockSize));
        pairs.push_back ({ sc18Rms (r.left), sc18Rms (r.right) });
        INFO ("mode " << idx << " L=" << pairs.back().l << " R=" << pairs.back().r);
        CHECK (pairs.back().l > 0.0f);
    }
    REQUIRE (pairs.size() == 5);
    for (size_t a = 0; a < pairs.size(); ++a)
        for (size_t b = a + 1; b < pairs.size(); ++b)
        {
            INFO ("modes " << a << " and " << b);
            CHECK (std::max (std::abs (pairs[a].l - pairs[b].l), std::abs (pairs[a].r - pairs[b].r)) > 1.0e-3f);
        }
}

TEST_CASE ("RenderEngine: Equal Power centres at azimuth 0 and hard-pans left at +90 (SC-18 part 2)", "[engine][sc18]")
{
    const auto centre = sc18StereoRender (kAlgorithmIndexEqualPower, 0.0f);
    CHECK_THAT (sc18Rms (centre.left), WithinAbs (sc18Rms (centre.right), 1.0e-6));
    CHECK (sc18Rms (centre.left) > 0.0f);

    // Positive azimuth = left.
    const auto left = sc18StereoRender (kAlgorithmIndexEqualPower, 90.0f);
    CHECK_THAT (sc18Rms (left.right), WithinAbs (0.0, 1.0e-6));
    CHECK (sc18Rms (left.left) > 0.0f);
}

TEST_CASE ("RenderEngine: a block distance gain of 0.5 halves both stereo channels in every mode (SC-18 part 2)",
           "[engine][sc18]")
{
    // Distance enters the stereo gains at block rate (RESEARCH Pitfall 9).
    for (int idx = kAlgorithmIndexEqualPower; idx <= kAlgorithmIndexBlumlein; ++idx)
    {
        INFO ("mode " << idx);
        const auto full = sc18StereoRender (idx, 20.0f, 1.0f);
        const auto half = sc18StereoRender (idx, 20.0f, 0.5f);
        REQUIRE (sc18Rms (full.left) > 0.0f);
        REQUIRE (sc18Rms (full.right) > 0.0f);
        CHECK_THAT (sc18Rms (half.left) / sc18Rms (full.left), WithinAbs (0.5, 1.0e-5));
        CHECK_THAT (sc18Rms (half.right) / sc18Rms (full.right), WithinAbs (0.5, 1.0e-5));
    }
}

TEST_CASE ("RenderEngine: a speaker index on the Stereo format renders as Equal Power; consumer stereo gains stay honoured with the flag off (SC-18 part 2)",
           "[engine][sc18]")
{
    const auto equalPower = sc18StereoRender (kAlgorithmIndexEqualPower, 20.0f);
    for (int idx = 0; idx < kNumSpeakerAlgorithmIndices; ++idx)
    {
        INFO ("speaker index " << idx);
        const auto r = sc18StereoRender (idx, 20.0f);
        CHECK (r.left == equalPower.left);
        CHECK (r.right == equalPower.right);
    }

    // engineComputesGains false: the consumer's own objGainL / objGainR are used verbatim.
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Stereo);
    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    RenderBlockContext ctx;
    ctx.sampleRate = kSampleRate;
    ctx.engineDerivesDispatch = true;
    ctx.objGainL[0] = 0.7f;
    ctx.objGainR[0] = 0.3f;
    std::vector<float> outL (kBlockSize, 0.0f), outR (kBlockSize, 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };
    for (int block = 0; block < 32; ++block)
        engine.renderBlock (sources, ctx, outPtrs, 2);
    CHECK_THAT (sc18Rms (outL), WithinAbs (0.5 * 0.7, 1.0e-5));
    CHECK_THAT (sc18Rms (outR), WithinAbs (0.5 * 0.3, 1.0e-5));
}

// ----------------------------------------------------------------------------
// SC-17 / SpatialCore#24 ("A+"): getBlockLayout() is the render-thread view of
// the layout, and the writer-view accessors count misuse from the render thread.
// ----------------------------------------------------------------------------
TEST_CASE ("RenderEngine: getBlockLayout follows the layout the last block rendered, never a newer one (SC-17)",
           "[engine][sc17]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    // Fresh engine: the reader's initial slot, the default Binaural layout.
    CHECK (engine.getBlockLayout().format == OutputFormat::Binaural);

    // A switch before any render does not move the render-thread view: only
    // renderBlock() acquires.
    engine.setOutputFormat (OutputFormat::Surround7_1_4);
    CHECK (engine.getBlockLayout().format == OutputFormat::Binaural);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    const RenderBlockContext ctx = makeSc16Context (true);
    Sc16Output out;

    out.clear();
    engine.renderBlock (sources, ctx, out.ptrs, kSc16MaxCh);
    CHECK (engine.getBlockLayout().format == OutputFormat::Surround7_1_4);

    // Two more switches without a render leave the view where the last block put it.
    engine.setOutputFormat (OutputFormat::Quad);
    engine.setOutputFormat (OutputFormat::Surround5_1);
    CHECK (engine.getBlockLayout().format == OutputFormat::Surround7_1_4);
    CHECK (engine.getBlockLayout().format == OutputFormat::Surround7_1_4);   // asking twice never acquires

    out.clear();
    engine.renderBlock (sources, ctx, out.ptrs, kSc16MaxCh);
    CHECK (engine.getBlockLayout().format == OutputFormat::Surround5_1);
    CHECK (engine.getBlockLayout().layout.numSpeakers == engine.getActiveLayout().layout.numSpeakers);
}

TEST_CASE ("RenderEngine: a single thread that writes and renders is never counted as writer-view misuse (SC-17)",
           "[engine][sc17]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    const RenderBlockContext ctx = makeSc16Context (true);
    Sc16Output out;

    engine.setOutputFormat (OutputFormat::Surround7_1_4);
    out.clear();
    engine.renderBlock (sources, ctx, out.ptrs, kSc16MaxCh);

    CHECK (engine.getActiveOutputFormat() == OutputFormat::Surround7_1_4);
    CHECK (engine.getActiveLayout().format == OutputFormat::Surround7_1_4);
    CHECK (engine.getWriterViewOnRenderThreadCount() == 0);
}

TEST_CASE ("RenderEngine: the writer view called from the render thread is counted (SC-17)",
           "[engine][sc17]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    const RenderBlockContext ctx = makeSc16Context (true);

    // This thread is the writer.
    engine.setOutputFormat (OutputFormat::Surround7_1_4);
    CHECK (engine.getWriterViewOnRenderThreadCount() == 0);

    // A different thread renders, then (wrongly) reads the writer view once.
    // Debug builds also jassert here; JUCE logs and only breaks under a debugger.
    std::thread renderThread ([&]
    {
        Sc16Output out;
        out.clear();
        engine.renderBlock (sources, ctx, out.ptrs, kSc16MaxCh);
        (void) engine.getActiveLayout();
    });
    renderThread.join();

    CHECK (engine.getWriterViewOnRenderThreadCount() == 1);

    // The writer thread reading its own view is still not counted.
    (void) engine.getActiveOutputFormat();
    CHECK (engine.getWriterViewOnRenderThreadCount() == 1);
}

TEST_CASE ("RenderEngine: a render thread always sees one of the formats a writer toggles between (SC-17 smoke)",
           "[engine][sc17]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setOutputFormat (OutputFormat::Surround7_1_4);

    SourceFixture fixture;
    RenderSources sources = fixture.makeSources();
    const RenderBlockContext ctx = makeSc16Context (true);

    std::atomic<bool> writerDone { false };
    std::atomic<int> unexpected { 0 };
    std::atomic<int> blocks { 0 };

    std::thread renderThread ([&]
    {
        Sc16Output out;
        while (! writerDone.load (std::memory_order_acquire))
        {
            out.clear();
            engine.renderBlock (sources, ctx, out.ptrs, kSc16MaxCh);
            const auto f = engine.getBlockLayout().format;
            if (f != OutputFormat::Surround7_1_4 && f != OutputFormat::Binaural
                && f != OutputFormat::AmbisonicsHOA)
                unexpected.fetch_add (1);
            blocks.fetch_add (1);
        }
    });

    const OutputFormat formats[3] = { OutputFormat::Surround7_1_4, OutputFormat::Binaural,
                                      OutputFormat::AmbisonicsHOA };
    for (int i = 0; i < 2000; ++i)
        engine.setOutputFormat (formats[i % 3]);
    writerDone.store (true, std::memory_order_release);
    renderThread.join();

    CHECK (unexpected.load() == 0);
    CHECK (blocks.load() > 0);
}

// ============================================================================
// SC-19 / SpatialCore#21 — a mid-stream Ambisonics order change must filter
// the newly active orders exactly like an engine that started at that order,
// and no NFC filter may change order (reallocate state) on the audio thread.
// ============================================================================
namespace
{
    // One live object (azimuth 30, elevation 20, distance 0.5 = 5 m, DC 0.5)
    // rendered through the Ambisonics path with explicit consumer flags.
    struct Sc19Rig
    {
        static constexpr int kMaxCh = 49; // 6th order

        RenderEngine engine;
        SourceFixture fixture;
        RenderSources sources;
        RenderBlockContext ctx;
        std::vector<std::vector<float>> outStorage;
        float* outPtrs[kMaxCh] = {};

        Sc19Rig()
            : outStorage (kMaxCh, std::vector<float> (kBlockSize, 0.0f))
        {
            engine.prepare (kSampleRate, kBlockSize);
            engine.setOutputFormat (OutputFormat::AmbisonicsHOA);
            sources = fixture.makeSources();
            sources.objects[0].azimuthDeg = 30.0f;
            sources.objects[0].elevationDeg = 20.0f;
            sources.objects[0].distance = 0.5f;
            ctx.sampleRate = kSampleRate;
            ctx.isAmbiOutput = true;
            for (int c = 0; c < kMaxCh; ++c)
                outPtrs[c] = outStorage[static_cast<size_t> (c)].data();
        }

        void render (int order, int blocks)
        {
            ctx.ambiOrder = order;
            const int numCh = (order + 1) * (order + 1);
            for (int b = 0; b < blocks; ++b)
            {
                for (auto& ch : outStorage)
                    std::fill (ch.begin(), ch.end(), 0.0f);
                engine.renderBlock (sources, ctx, outPtrs, numCh);
            }
        }

        float lastBlockMean (int acn) const
        {
            const auto& ch = outStorage[static_cast<size_t> (acn)];
            double sum = 0.0;
            for (float v : ch)
                sum += v;
            return static_cast<float> (sum / static_cast<double> (ch.size()));
        }

        bool lastBlockFinite (int numCh) const
        {
            for (int c = 0; c < numCh; ++c)
                if (! allFinite (outStorage[static_cast<size_t> (c)].data(), kBlockSize))
                    return false;
            return true;
        }
    };

    // Largest |mean| of B over ACN [first, last], and the largest A-B gap.
    void sc19Compare (const Sc19Rig& a, const Sc19Rig& b, int first, int last,
                      float& scale, float& worstGap, int& worstAcn)
    {
        scale = 0.0f;
        worstGap = 0.0f;
        worstAcn = first;
        for (int acn = first; acn <= last; ++acn)
        {
            const float mb = b.lastBlockMean (acn);
            scale = std::max (scale, std::abs (mb));
            const float gap = std::abs (a.lastBlockMean (acn) - mb);
            if (gap > worstGap)
            {
                worstGap = gap;
                worstAcn = acn;
            }
        }
    }
}

TEST_CASE ("RenderEngine: raising the Ambisonics order mid-stream filters the new orders like a fresh 3rd-order engine (SC-19, SpatialCore#21)",
           "[engine][sc19]")
{
    Sc19Rig a, b;
    a.render (1, 200);
    a.render (3, 200);
    b.render (3, 400);

    float scale = 0.0f, worstGap = 0.0f;
    int worstAcn = 4;
    sc19Compare (a, b, 4, 15, scale, worstGap, worstAcn);

    // RED evidence: print the ACN 4..15 means of both engines.
    for (int acn = 4; acn <= 15; ++acn)
        std::printf ("SC19-MEANS acn=%d A=%.6f B=%.6f\n", acn, a.lastBlockMean (acn), b.lastBlockMean (acn));
    std::printf ("SC19-RESULT scale=%.6f worstGap=%.6f worstAcn=%d\n", scale, worstGap, worstAcn);
    std::fflush (stdout);

    REQUIRE (scale > 1e-4f); // non-vacuous
    CHECK (a.lastBlockFinite (16));
    CHECK (worstGap <= 1e-3f * scale);
}

TEST_CASE ("RenderEngine: lowering then raising the Ambisonics order matches a fresh 6th-order engine on ACN 4..48 (SC-19, SpatialCore#21)",
           "[engine][sc19]")
{
    Sc19Rig a, b;
    a.render (3, 150);
    a.render (1, 150);
    a.render (6, 200);
    b.render (6, 500);

    float scale = 0.0f, worstGap = 0.0f;
    int worstAcn = 4;
    sc19Compare (a, b, 4, 48, scale, worstGap, worstAcn);
    std::printf ("SC19-RESULT-6 scale=%.6f worstGap=%.6f worstAcn=%d\n", scale, worstGap, worstAcn);
    std::fflush (stdout);

    REQUIRE (scale > 1e-4f);
    CHECK (a.lastBlockFinite (49));
    CHECK (worstGap <= 1e-3f * scale);
}
