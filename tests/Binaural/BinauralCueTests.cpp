#include <catch2/catch_test_macros.hpp>
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

    SECTION ("front vs back")
    {
        const auto back = engineImpulseResponse (engine, BinauralPath::Simple, { 180.0f, 0.0f }, kSampleRate);
        REQUIRE (signalIsFinite (back));
        const auto d = bandDifference (front, back, kSampleRate);
        WARN ("Simple front vs back: rms " << d.rmsDb << " dB, max " << d.maxDb << " dB");
        CHECK (d.rmsDb >= kMinRmsDb);
        CHECK (d.maxDb >= kMinMaxDb);
    }
}
