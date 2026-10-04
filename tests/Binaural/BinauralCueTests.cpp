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
