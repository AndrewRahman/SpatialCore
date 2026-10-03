#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>
#include "../Binaural/BinauralTestUtilities.h"
#include <algorithm>
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
    // rounding, and no tolerance can resolve it.
    //
    // Hence kAmbiPinTolerance = 2.5e-5: about 2.5x above the worst rounding
    // spread and 2.5x below the +0.1% change. kAmbiFloatVsDoubleTolerance =
    // 4e-5 is about 2.2x the worst float-vs-double distance. It re-checks the
    // premise above on every build, so a toolchain that moves the noise floor
    // fails there with a message that says so, instead of looking like a
    // library change.
    //
    // The previous bound (1e-6) came from RESEARCH F10, a Debug-grade "worst 0"
    // measurement plus an unmeasured margin. The full derivation is in
    // .planning/debug/ambi-pin-release-tolerance.md (G-02-10).
    constexpr float kAmbiPinTolerance = 2.5e-5f;
    constexpr double kAmbiFloatVsDoubleTolerance = 4.0e-5;

    // Folds one distance into a running maximum. std::max (a, b) is
    // (a < b) ? b : a, so a NaN distance would be silently dropped and never
    // reach a tolerance CHECK (WR-06). A non-finite distance is therefore
    // rejected here: it returns false and leaves the maximum untouched, so the
    // caller can count it and fail loudly.
    template <typename T>
    bool accumulateWorstFinite (T& worst, T distance)
    {
        if (! std::isfinite (distance))
            return false;
        worst = std::max (worst, distance);
        return true;
    }

    // The same decode as referenceAmbiDecode, in double, on the same float
    // inputs: E is bit-identical to what both float decoders see, and the
    // Tikhonov term is the float epsilon promoted, so only precision differs.
    // Fixed-size stack arrays, no heap. The caller zero-initialises outMatrix.
    void doublePrecisionAmbiDecode (const SpeakerLayout& layout,
                                    double (*outMatrix)[MAX_SPEAKERS])
    {
        constexpr int M = 16;
        const int N = layout.numSpeakers;

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

            const double diag = aug[col][col];
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
