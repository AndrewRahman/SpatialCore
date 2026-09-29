#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <SpatialCore/Trajectory/DopplerVelocity.h>
#include <cmath>

using namespace spatialcore;

namespace
{
    constexpr float kSampleRate = 48000.0;
    constexpr int   kBlockSize  = 512;
    constexpr float kBlockDur   = static_cast<float> (kBlockSize) / kSampleRate;
}

// ============================================================================
// Deeper DopplerVelocity coverage (D-09 fresh-test-separate-commit), replicating
// Tests/DSPUnitTests.cpp's "Section 2: DopplerVelocity Tests" style but calling
// spatialcore::DopplerVelocity:: directly.
// ============================================================================

TEST_CASE ("DopplerVelocity: stationary object produces 0 semitones", "[doppler][basic]")
{
    DopplerVelocity dv;
    dv.resetAll();  // resets prevAz/prevEl/prevDist to (0, 0, 0.5)

    for (int b = 0; b < 30; ++b)
    {
        dv.update (0, 0.0f, 0.0f, 0.5f, 1.0f, kBlockDur);
        dv.smooth (0);
    }

    REQUIRE (dv.getSmoothedSemitones (0) == Catch::Approx (0.0f).margin (0.001f));
}

TEST_CASE ("DopplerVelocity: approaching object produces positive pitch shift", "[doppler][basic]")
{
    DopplerVelocity dv;
    dv.resetAll();

    for (int b = 0; b < 30; ++b)
    {
        float dist = 0.9f - static_cast<float> (b) * 0.025f;
        dv.update (0, 0.0f, 0.0f, dist, 1.0f, kBlockDur);
        dv.smooth (0);
    }

    REQUIRE (dv.getSmoothedSemitones (0) > 0.0f);
}

TEST_CASE ("DopplerVelocity: receding object produces negative pitch shift", "[doppler][basic]")
{
    DopplerVelocity dv;
    dv.resetAll();

    for (int b = 0; b < 30; ++b)
    {
        float dist = 0.1f + static_cast<float> (b) * 0.025f;
        dv.update (0, 0.0f, 0.0f, dist, 1.0f, kBlockDur);
        dv.smooth (0);
    }

    REQUIRE (dv.getSmoothedSemitones (0) < 0.0f);
}

TEST_CASE ("DopplerVelocity: EMA smoothing eliminates step artifacts", "[doppler][smoothing]")
{
    DopplerVelocity dv;
    dv.resetAll();

    dv.update (0, 0.0f, 0.0f, 0.5f, 1.0f, kBlockDur);
    dv.smooth (0);
    dv.update (0, 3.14159265f, 0.0f, 0.5f, 1.0f, kBlockDur);
    dv.smooth (0);

    float prevVal = dv.getSmoothedSemitones (0);
    float maxDelta = 0.0f;
    for (int b = 0; b < 30; ++b)
    {
        dv.update (0, 3.14159265f, 0.0f, 0.5f, 1.0f, kBlockDur);
        dv.smooth (0);
        float val = dv.getSmoothedSemitones (0);
        maxDelta = std::max (maxDelta, std::abs (val - prevVal));
        prevVal = val;
    }

    REQUIRE (maxDelta < 1.0f);
}

TEST_CASE ("DopplerVelocity: reset() zeroes all state", "[doppler][reset]")
{
    DopplerVelocity dv;
    dv.resetAll();

    for (int b = 0; b < 10; ++b)
    {
        float dist = 0.9f - static_cast<float> (b) * 0.05f;
        dv.update (0, 0.0f, 0.0f, dist, 1.0f, kBlockDur);
        dv.smooth (0);
    }
    REQUIRE (std::abs (dv.getRawSemitones (0)) > 0.0f);

    dv.reset (0, 0.0f, 0.0f, 0.5f);

    REQUIRE (dv.getRawSemitones (0) == 0.0f);
    REQUIRE (dv.getSmoothedSemitones (0) == 0.0f);
    REQUIRE (dv.getPrevSmoothedSemitones (0) == 0.0f);
    REQUIRE (dv.positionChanged (0) == false);
}

TEST_CASE ("DopplerVelocity: clearDisabled() zeroes raw semitones", "[doppler]")
{
    DopplerVelocity dv;
    dv.resetAll();

    for (int b = 0; b < 10; ++b)
    {
        float dist = 0.9f - static_cast<float> (b) * 0.05f;
        dv.update (0, 0.0f, 0.0f, dist, 1.0f, kBlockDur);
        dv.smooth (0);
    }

    dv.clearDisabled (0);
    REQUIRE (dv.getRawSemitones (0) == 0.0f);
}

TEST_CASE ("DopplerVelocity: zero dopplerAmount produces 0 semitones", "[doppler]")
{
    DopplerVelocity dv;
    dv.resetAll();

    for (int b = 0; b < 20; ++b)
    {
        float dist = 0.9f - static_cast<float> (b) * 0.03f;
        dv.update (0, 0.0f, 0.0f, dist, 0.0f, kBlockDur);
        dv.smooth (0);
    }

    REQUIRE (dv.getRawSemitones (0) == 0.0f);
}

TEST_CASE ("DopplerVelocity: semitones clamped to +/-12", "[doppler]")
{
    DopplerVelocity dv;
    dv.resetAll();

    constexpr float tinyBlockDur = 0.0001f;
    dv.update (0, 0.0f, 0.0f, 0.01f, 1.0f, tinyBlockDur);
    dv.update (0, 0.0f, 0.0f, 0.99f, 1.0f, tinyBlockDur);

    REQUIRE (std::abs (dv.getRawSemitones (0)) <= 12.0f);
}

TEST_CASE ("DopplerVelocity: static position yields zero semitones (08-02 smoke retained)", "[doppler]")
{
    DopplerVelocity dv;
    dv.reset (0, 0.0f, 0.0f, 0.5f);
    dv.update (0, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f / 60.0f);
    dv.smooth (0);
    REQUIRE (dv.getSmoothedSemitones (0) == 0.0f);
}
