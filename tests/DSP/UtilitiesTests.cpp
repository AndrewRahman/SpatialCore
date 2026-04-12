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

TEST_CASE("outputLimiter: zero passthrough", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(0.0f) == Approx(0.0f));
}

TEST_CASE("outputLimiter: near-passthrough for small values", "[dsp]")
{
    // tanh(x/c)*c is approximately x for small x (within ~5% for |x| < 0.5)
    REQUIRE(spatialcore::DSP::outputLimiter(0.5f) == Approx(0.5f).margin(0.05f));
}

TEST_CASE("outputLimiter: asymptotically approaches +2dB ceiling", "[dsp]")
{
    float ceiling = 1.2589f;
    float result = spatialcore::DSP::outputLimiter(5.0f);
    REQUIRE(result > ceiling * 0.99f);
    REQUIRE(result <= ceiling);

    float resultNeg = spatialcore::DSP::outputLimiter(-5.0f);
    REQUIRE(resultNeg < -ceiling * 0.99f);
    REQUIRE(resultNeg >= -ceiling);
}

TEST_CASE("outputLimiter: odd symmetry", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(2.0f) == Approx(-spatialcore::DSP::outputLimiter(-2.0f)));
}

TEST_CASE("outputLimiter: NaN returns zero", "[dsp]")
{
    REQUIRE(spatialcore::DSP::outputLimiter(std::nanf("")) == 0.0f);
}
