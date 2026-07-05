#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Core/SpatialMath.h>
#include <cmath>
#include <limits>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// softClip / outputLimiter / distanceAttenuation -- deeper coverage (D-09
// fresh-test-separate-commit), asserting the exact constants (threshold 0.8
// for softClip, 1.2589 for outputLimiter) and the golden distance-attenuation
// value captured from the pre-consolidation OSD build, so these tests prove
// byte-identical behavior rather than merely plausible behavior. Mirrors the
// verbatim bodies at Source/PluginProcessor.h:1083-1104 and the two verified
// byte-identical formula sites at Source/PluginProcessor.cpp:3091 and :4500.
// ============================================================================

TEST_CASE ("softClip: non-finite input returns 0.0", "[spatialmath][softclip]")
{
    CHECK (softClip (std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    CHECK (softClip (std::numeric_limits<float>::infinity()) == 0.0f);
    CHECK (softClip (-std::numeric_limits<float>::infinity()) == 0.0f);
}

TEST_CASE ("softClip: below threshold is pass-through", "[spatialmath][softclip]")
{
    CHECK_THAT (softClip (0.5f), WithinAbs (0.5f, 1e-6f));
    CHECK_THAT (softClip (-0.5f), WithinAbs (-0.5f, 1e-6f));
    CHECK_THAT (softClip (0.0f), WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("softClip: exact threshold boundary is pass-through", "[spatialmath][softclip]")
{
    // x == threshold does not satisfy x > threshold, so it falls through to pass-through.
    CHECK_THAT (softClip (0.8f), WithinAbs (0.8f, 1e-6f));
    CHECK_THAT (softClip (-0.8f), WithinAbs (-0.8f, 1e-6f));
}

TEST_CASE ("softClip: above threshold applies exact asymptotic formula", "[spatialmath][softclip]")
{
    const float threshold = 0.8f;
    const float x = 1.0f;
    const float expected = threshold + (x - threshold) / (1.0f + (x - threshold) * (x - threshold));
    CHECK_THAT (softClip (x), WithinAbs (expected, 1e-6f));

    const float xNeg = -1.0f;
    const float expectedNeg = -threshold + (xNeg + threshold) / (1.0f + (xNeg + threshold) * (xNeg + threshold));
    CHECK_THAT (softClip (xNeg), WithinAbs (expectedNeg, 1e-6f));
}

TEST_CASE ("softClip: odd symmetry", "[spatialmath][softclip]")
{
    CHECK_THAT (softClip (1.5f), WithinAbs (-softClip (-1.5f), 1e-6f));
    CHECK_THAT (softClip (3.0f), WithinAbs (-softClip (-3.0f), 1e-6f));
}

TEST_CASE ("softClip: asymptotically bounded, never reaches 1.0", "[spatialmath][softclip]")
{
    float result = softClip (1000.0f);
    CHECK (result > 0.8f);
    CHECK (result < 1.0f);
}

TEST_CASE ("outputLimiter: non-finite input returns 0.0", "[spatialmath][outputlimiter]")
{
    CHECK (outputLimiter (std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    CHECK (outputLimiter (std::numeric_limits<float>::infinity()) == 0.0f);
    CHECK (outputLimiter (-std::numeric_limits<float>::infinity()) == 0.0f);
}

TEST_CASE ("outputLimiter: matches exact 1.2589 * tanh(x/1.2589) formula", "[spatialmath][outputlimiter]")
{
    const float threshold = 1.2589f;
    for (float x : { -5.0f, -1.2589f, -0.5f, 0.0f, 0.5f, 1.2589f, 2.0f, 5.0f, 100.0f })
    {
        float expected = threshold * std::tanh (x / threshold);
        CHECK_THAT (outputLimiter (x), WithinAbs (expected, 1e-6f));
    }
}

TEST_CASE ("outputLimiter: odd symmetry", "[spatialmath][outputlimiter]")
{
    CHECK_THAT (outputLimiter (2.0f), WithinAbs (-outputLimiter (-2.0f), 1e-6f));
}

TEST_CASE ("outputLimiter: asymptotically approaches +/-1.2589 ceiling", "[spatialmath][outputlimiter]")
{
    const float ceiling = 1.2589f;
    float result = outputLimiter (1000.0f);
    CHECK (result > ceiling * 0.999f);
    CHECK (result <= ceiling);

    float resultNeg = outputLimiter (-1000.0f);
    CHECK (resultNeg < -ceiling * 0.999f);
    CHECK (resultNeg >= -ceiling);
}

TEST_CASE ("distanceAttenuation: matches pre-consolidation golden value", "[spatialmath][distance]")
{
    // Golden values captured from the pre-move OSD formula:
    // 1.0f / std::max(0.1f, distance * 4.0f + 0.25f)
    // (verified byte-identical at Source/PluginProcessor.cpp:3091 and :4500
    // before this consolidation).
    CHECK_THAT (distanceAttenuation (0.0f), WithinAbs (1.0f / 0.25f, 1e-6f));
    CHECK_THAT (distanceAttenuation (0.5f), WithinAbs (1.0f / 2.25f, 1e-6f));
    CHECK_THAT (distanceAttenuation (1.0f), WithinAbs (1.0f / 4.25f, 1e-6f));
    CHECK_THAT (distanceAttenuation (2.0f), WithinAbs (1.0f / 8.25f, 1e-6f));
}

TEST_CASE ("distanceAttenuation: clamps denominator floor at 0.1", "[spatialmath][distance]")
{
    // Negative distance would drive (distance*4+0.25) below 0.1 without the
    // std::max clamp; verify the clamp is active.
    CHECK_THAT (distanceAttenuation (-1.0f), WithinAbs (1.0f / 0.1f, 1e-6f));
}

TEST_CASE ("distanceAttenuation: monotonically decreasing with distance", "[spatialmath][distance]")
{
    float g0 = distanceAttenuation (0.0f);
    float g1 = distanceAttenuation (1.0f);
    float g2 = distanceAttenuation (5.0f);
    CHECK (g0 > g1);
    CHECK (g1 > g2);
}
