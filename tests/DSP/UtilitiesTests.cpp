#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <SpatialCore/DSP/Utilities.h>
#include <cmath>

using Catch::Approx;

TEST_CASE("softClip: passthrough below threshold", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(0.0f) == Approx(0.0f));
    REQUIRE(spatialcore::DSP::softClip(0.5f) == Approx(0.5f));
    REQUIRE(spatialcore::DSP::softClip(-0.5f) == Approx(-0.5f));
    REQUIRE(spatialcore::DSP::softClip(0.79f) == Approx(0.79f));
}

TEST_CASE("softClip: saturates above threshold", "[dsp]")
{
    float result = spatialcore::DSP::softClip(2.0f);
    REQUIRE(result > 0.8f);
    REQUIRE(result < 2.0f);
}

TEST_CASE("softClip: odd symmetry", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(1.5f) == Approx(-spatialcore::DSP::softClip(-1.5f)));
}

TEST_CASE("softClip: NaN returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(std::nanf("")) == 0.0f);
}

TEST_CASE("softClip: infinity returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::softClip(std::numeric_limits<float>::infinity()) == 0.0f);
    REQUIRE(spatialcore::DSP::softClip(-std::numeric_limits<float>::infinity()) == 0.0f);
}

TEST_CASE("outputLimiter: passthrough below ceiling", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(0.0f) == Approx(0.0f));
    REQUIRE(spatialcore::DSP::outputLimiter(0.5f) == Approx(0.5f));
    REQUIRE(spatialcore::DSP::outputLimiter(1.0f) == Approx(1.0f));
}

TEST_CASE("outputLimiter: clamps at +2dB ceiling", "[dsp]")
{
    float ceiling = 1.2589f;
    REQUIRE(spatialcore::DSP::outputLimiter(5.0f) == Approx(ceiling));
    REQUIRE(spatialcore::DSP::outputLimiter(-5.0f) == Approx(-ceiling));
}

TEST_CASE("outputLimiter: NaN returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(std::nanf("")) == 0.0f);
}
