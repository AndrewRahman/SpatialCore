#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include "../Binaural/BinauralTestUtilities.h"
#include <cmath>
#include <limits>
#include <memory>
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

// ----------------------------------------------------------------------------
// D-06(a) extended by D-19(ii): RenderEngine holds the last finite azimuth,
// elevation and distance per object, field by field, and substitutes it for
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

        void render (float azDeg, float elDeg, float dist)
        {
            for (auto& ch : out)
                std::fill (ch.begin(), ch.end(), 0.0f);

            RenderSources s = fixture.makeSources();
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
