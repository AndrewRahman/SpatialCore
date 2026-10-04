#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "BinauralMetrics.h"

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// BUG-01, HRTF half (D-03, SpatialCore#15): the HRTF path must separate front from
// back and front from overhead on every built-in profile. The oracle is the engine's
// own impulse response, so it measures what a consumer hears, not what the database
// returns. "Distinguishable" = third-octave RMS band difference >= 2.0 dB AND largest
// band difference >= 4.0 dB. The bounds sit below the weakest measured shipped pair
// (SADIE 2.90 / 6.24 and 3.05 / 7.09, 03-RESEARCH.md) and were set before this test
// ran; do not loosen them to make a measurement pass.
// ============================================================================

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kMinRmsDb = 2.0;
    constexpr double kMinMaxDb = 4.0;
    constexpr const char* kProfileNames[] = { "Simple", "SADIE", "CIPIC", "HUTUBS", "Bernschuetz", "KEMAR" };
}

TEST_CASE ("BUG-01: the HRTF path separates front, back and overhead on every built-in profile",
           "[bug01][hrtf]")
{
    for (int profile = 1; profile <= 5; ++profile)
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        REQUIRE (loadProfileIntoActiveRenderer (engine, profile, kSampleRate));

        const auto front = engineImpulseResponse (engine, BinauralPath::HRTF, { 0.0f, 0.0f }, kSampleRate);
        const auto back = engineImpulseResponse (engine, BinauralPath::HRTF, { 180.0f, 0.0f }, kSampleRate);
        const auto up = engineImpulseResponse (engine, BinauralPath::HRTF, { 0.0f, 90.0f }, kSampleRate);
        REQUIRE (signalIsFinite (front));
        REQUIRE (signalIsFinite (back));
        REQUIRE (signalIsFinite (up));

        const auto frontVsBack = bandDifference (front, back, kSampleRate);
        const auto frontVsUp = bandDifference (front, up, kSampleRate);

        WARN ("profile " << profile << " " << kProfileNames[profile]
              << ": front-vs-back rms " << frontVsBack.rmsDb << " dB, max " << frontVsBack.maxDb
              << " dB; front-vs-up90 rms " << frontVsUp.rmsDb << " dB, max " << frontVsUp.maxDb << " dB");

        INFO ("profile " << profile << " " << kProfileNames[profile] << " front vs back: rms "
              << frontVsBack.rmsDb << " dB, max " << frontVsBack.maxDb << " dB");
        CHECK (frontVsBack.rmsDb >= kMinRmsDb);
        CHECK (frontVsBack.maxDb >= kMinMaxDb);

        INFO ("profile " << profile << " " << kProfileNames[profile] << " front vs up90: rms "
              << frontVsUp.rmsDb << " dB, max " << frontVsUp.maxDb << " dB");
        CHECK (frontVsUp.rmsDb >= kMinRmsDb);
        CHECK (frontVsUp.maxDb >= kMinMaxDb);
    }
}

// ============================================================================
// BUG-01, Simple half (D-01, D-03, SpatialCore#15): the Simple (Woodworth) path used to
// be two broadband gains driven by the lateral angle alone, so a source behind, in front,
// overhead and underfoot all sounded identical (0.00 dB measured). RenderEngine now runs a
// position-blended rear/up/down filter bank (Core/SimpleBinauralCues.h) before the pan
// gains. Same bounds as the HRTF half; the oracle is again the engine impulse response.
// ============================================================================

TEST_CASE ("BUG-01: the Simple path separates front from back and overhead", "[bug01][simple]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, 512);
    engine.setOutputFormat (OutputFormat::Binaural);

    const auto front = engineImpulseResponse (engine, BinauralPath::Simple, { 0.0f, 0.0f }, kSampleRate);

    auto measure = [&] (const char* label, Direction dir)
    {
        const auto ir = engineImpulseResponse (engine, BinauralPath::Simple, dir, kSampleRate);
        REQUIRE (signalIsFinite (ir));
        const auto d = bandDifference (front, ir, kSampleRate);
        WARN ("Simple front vs " << label << ": rms " << d.rmsDb << " dB, max " << d.maxDb << " dB");
        return d;
    };

    SECTION ("front vs back")
    {
        const auto d = measure ("back", { 180.0f, 0.0f });
        CHECK (d.rmsDb >= kMinRmsDb);
        CHECK (d.maxDb >= kMinMaxDb);
    }

    SECTION ("front vs up90")
    {
        const auto d = measure ("up90", { 0.0f, 90.0f });
        CHECK (d.rmsDb >= kMinRmsDb);
        CHECK (d.maxDb >= kMinMaxDb);
    }

    SECTION ("front vs up45")
    {
        const auto d = measure ("up45", { 0.0f, 45.0f });
        CHECK (d.maxDb >= 2.0);
    }

    SECTION ("front vs down45")
    {
        const auto d = measure ("down45", { 0.0f, -45.0f });
        CHECK (d.maxDb >= 2.0);
    }

    SECTION ("front vs down90")
    {
        const auto d = measure ("down90", { 0.0f, -90.0f });
        CHECK (d.maxDb >= 4.0);
    }
}

// ============================================================================
// D-01 "recognisably close at ear level, front": every Simple-path source at ear level in
// the front half (|az| <= 90, including exactly +-90, where the float cosine is -4.4e-8
// and only the weight snap keeps the rear weight at 0) renders bit-identical to the
// pre-change formula, mono x pan gain x tap fade. The consumer supplies constant gains
// here (engineComputesGains false), so blocks 2 and 3 have no gain ramp and the
// expectation is exact.
// ============================================================================

TEST_CASE ("BUG-01: ear-level front sources on the Simple path are bit-identical to the old formula",
           "[bug01][identity]")
{
    constexpr int kBlock = 128;
    const float azimuths[] = { 0.0f, 30.0f, 60.0f, 89.9f, 90.0f, -45.0f, -90.0f };

    for (float az : azimuths)
    {
        INFO ("azimuth " << az);
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlock);

        RenderBlockContext ctx;
        ctx.isBinaural = true;
        ctx.useHRTF = false;
        ctx.engineComputesGains = false;
        ctx.sampleRate = kSampleRate;
        ctx.objGains[0].leftGain = 0.71f;
        ctx.objGains[0].rightGain = 0.29f;

        const auto mono = whiteNoise (static_cast<size_t> (kBlock) * 3u, 1234u, 0.5f);
        std::vector<float> tapFade (kBlock, 1.0f), distGain (kBlock, 1.0f), blockL (kBlock), blockR (kBlock);
        float* outPtrs[2] = { blockL.data(), blockR.data() };

        for (int block = 0; block < 3; ++block)
        {
            RenderSources sources;
            sources.numSamples = kBlock;
            sources.monoBuffers[0] = mono.data() + block * kBlock;
            sources.tapFadeGainPerSample[0] = tapFade.data();
            sources.distGainPerSample[0] = distGain.data();
            sources.objectLive[0] = true;
            sources.objects[0].azimuthDeg = az;
            sources.objects[0].elevationDeg = 0.0f;
            sources.objects[0].distance = 0.5f;
            sources.objects[0].enabled = true;

            engine.renderBlock (sources, ctx, outPtrs, 2);

            if (block == 0)
                continue;   // block 1 ramps the pan gains up from zero

            int mismatches = 0;
            for (int i = 0; i < kBlock; ++i)
            {
                const float x = mono[static_cast<size_t> (block * kBlock + i)];
                if (blockL[static_cast<size_t> (i)] != x * 0.71f * 1.0f) ++mismatches;
                if (blockR[static_cast<size_t> (i)] != x * 0.29f * 1.0f) ++mismatches;
            }
            CHECK (mismatches == 0);
        }
    }
}

// ============================================================================
// The pure weight function, the filter table design and robustness.
// ============================================================================

TEST_CASE ("BUG-01: cue weights, branch designs and non-finite robustness", "[bug01][weights]")
{
    const auto rad = [] (float deg) { return juce::degreesToRadians (deg); };

    SECTION ("weight table")
    {
        auto w = computeSimpleCueWeights (rad (0.0f), rad (0.0f));
        CHECK (w.rear == 0.0f); CHECK (w.up == 0.0f); CHECK (w.down == 0.0f);

        w = computeSimpleCueWeights (rad (180.0f), rad (0.0f));
        CHECK (w.rear == Catch::Approx (1.0f).margin (1.0e-6)); CHECK (w.up == 0.0f); CHECK (w.down == 0.0f);

        // exactly +-90 at ear level: only the snap keeps the float cosine residue out
        for (float az : { 90.0f, -90.0f })
        {
            w = computeSimpleCueWeights (rad (az), rad (0.0f));
            CHECK (w.rear == 0.0f); CHECK (w.up == 0.0f); CHECK (w.down == 0.0f);
        }

        w = computeSimpleCueWeights (rad (0.0f), rad (90.0f));
        CHECK (w.up == Catch::Approx (1.0f).margin (1.0e-6)); CHECK (w.rear == 0.0f); CHECK (w.down == 0.0f);

        w = computeSimpleCueWeights (rad (0.0f), rad (-90.0f));
        CHECK (w.down == Catch::Approx (1.0f).margin (1.0e-6)); CHECK (w.rear == 0.0f); CHECK (w.up == 0.0f);

        w = computeSimpleCueWeights (rad (180.0f), rad (45.0f));
        CHECK (w.rear == Catch::Approx (0.7071f).margin (1.0e-4));
        CHECK (w.up == Catch::Approx (0.7071f).margin (1.0e-4));
        CHECK (w.down == 0.0f);
    }

    SECTION ("designed branch responses at 48 kHz")
    {
        auto cascadeDb = [] (const auto& stages, double freqHz)
        {
            double mag = 1.0;
            for (const auto& st : stages)
            {
                const float f = static_cast<float> (std::min (static_cast<double> (st.frequencyHz), 0.45 * kSampleRate));
                const float g = juce::Decibels::decibelsToGain (st.gainDb);
                const auto c = (st.kind == SimpleCueStage::Kind::HighShelf)
                    ? juce::dsp::IIR::Coefficients<float>::makeHighShelf (kSampleRate, f, st.q, g)
                    : juce::dsp::IIR::Coefficients<float>::makePeakFilter (kSampleRate, f, st.q, g);
                mag *= c->getMagnitudeForFrequency (freqHz, kSampleRate);
            }
            return 20.0 * std::log10 (mag);
        };

        WARN ("Rear dB at 1 Hz / 5 kHz: " << cascadeDb (kSimpleCueRearStages, 1.0) << " / " << cascadeDb (kSimpleCueRearStages, 5000.0));
        WARN ("Up dB at 1 Hz / 8 kHz: " << cascadeDb (kSimpleCueUpStages, 1.0) << " / " << cascadeDb (kSimpleCueUpStages, 8000.0));
        WARN ("Down dB at 1 Hz / 5.5 kHz: " << cascadeDb (kSimpleCueDownStages, 1.0) << " / " << cascadeDb (kSimpleCueDownStages, 5500.0));

        CHECK (std::abs (cascadeDb (kSimpleCueRearStages, 1.0)) < 0.01);
        CHECK (std::abs (cascadeDb (kSimpleCueUpStages, 1.0)) < 0.01);
        CHECK (std::abs (cascadeDb (kSimpleCueDownStages, 1.0)) < 0.01);

        CHECK (cascadeDb (kSimpleCueRearStages, 5000.0) == Catch::Approx (-4.0).margin (0.5));
        CHECK (cascadeDb (kSimpleCueUpStages, 8000.0) == Catch::Approx (8.4).margin (0.5));
        CHECK (cascadeDb (kSimpleCueDownStages, 5500.0) == Catch::Approx (-5.2).margin (0.5));
    }

    SECTION ("finite output for every direction on a 15 degree grid")
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        const RenderBlockContext ctx = makeBinauralContext (BinauralPath::Simple, kSampleRate);
        const auto noise = whiteNoise (2048, 99u, 0.5f);

        int nonFinite = 0;
        for (int az = -180; az <= 180; az += 15)
            for (int el = -90; el <= 90; el += 15)
            {
                const Direction d { static_cast<float> (az), static_cast<float> (el) };
                const auto out = renderThroughEngine (engine, ctx, noise, { 512 }, [d] (int64_t) { return d; });
                if (! signalIsFinite (out))
                    ++nonFinite;
            }
        CHECK (nonFinite == 0);
    }

    SECTION ("non-finite positions and input never produce non-finite output")
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        const RenderBlockContext ctx = makeBinauralContext (BinauralPath::Simple, kSampleRate);
        const auto noise = whiteNoise (4096, 7u, 0.5f);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();

        const auto out = renderThroughEngine (engine, ctx, noise, { 512 },
            [nan, inf] (int64_t start)
            {
                if (start < 1024) return Direction { 180.0f, nan };
                if (start < 2048) return Direction { nan, inf };
                return Direction { 180.0f, 30.0f };
            });
        CHECK (signalIsFinite (out));

        // one NaN input sample must not poison the recursive filter state for good
        auto bad = whiteNoise (4096, 8u, 0.5f);
        bad[100] = nan;
        const auto badOut = renderThroughEngine (engine, ctx, bad, { 512 }, [] (int64_t) { return Direction { 180.0f, 0.0f }; });
        bool tailFinite = true;
        for (size_t i = 1024; i < badOut.left.size(); ++i)
            tailFinite = tailFinite && std::isfinite (badOut.left[i]) && std::isfinite (badOut.right[i]);
        CHECK (tailFinite);
    }
}

// ============================================================================
// DR-1 stale-state guard: the cue filters hold recursive state. When the Simple path
// resumes after blocks that did not run it, that state must be zeroed, otherwise a
// decaying tail of old audio leaks into the first block.
// ============================================================================

TEST_CASE ("BUG-01: the Simple cue state is zeroed when the Simple path resumes", "[bug01][cold-start]")
{
    RenderEngine engine;
    engine.prepare (kSampleRate, 512);
    engine.setOutputFormat (OutputFormat::Binaural);

    const auto loud = whiteNoise (4 * 512, 5u, 0.8f);
    const auto behind = [] (int64_t) { return Direction { 180.0f, 0.0f }; };

    const auto simple = renderThroughEngine (engine, makeBinauralContext (BinauralPath::Simple, kSampleRate),
                                              loud, { 512 }, behind);
    REQUIRE (signalIsFinite (simple));

    // two blocks on the HRTF path (no profile needed: the Simple path just must not run)
    const std::vector<float> silence2 (2 * 512, 0.0f);
    renderThroughEngine (engine, makeBinauralContext (BinauralPath::HRTF, kSampleRate), silence2, { 512 }, behind);

    const std::vector<float> silence1 (512, 0.0f);
    const auto resumed = renderThroughEngine (engine, makeBinauralContext (BinauralPath::Simple, kSampleRate),
                                               silence1, { 512 }, behind);
    REQUIRE (signalIsFinite (resumed));

    float peak = 0.0f;
    for (size_t i = 0; i < resumed.left.size(); ++i)
        peak = std::max ({ peak, std::abs (resumed.left[i]), std::abs (resumed.right[i]) });
    CHECK (peak == 0.0f);
}
