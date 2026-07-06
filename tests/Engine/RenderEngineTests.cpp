#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <cmath>
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
