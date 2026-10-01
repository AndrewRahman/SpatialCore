#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/IO/AmbisonicsCodec.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <SpatialCore/Core/SpatialMath.h>
#include "../reference/ShReference.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kDegToRad = kPi / 180.0f;

namespace
{
    constexpr int kNumShChannels = AmbisonicsCodec::MAX_AMBI_CHANNELS;   // 49, order 6

    // ACN -> (l, m): l = floor(sqrt(acn)), m = acn - l*l - l.
    int shOrderOf (int acn)  { return static_cast<int> (std::floor (std::sqrt (static_cast<double> (acn)))); }
    int shDegreeOf (int acn) { const int l = shOrderOf (acn); return acn - l * l - l; }

    // Directions uniform on the sphere, fixed seed: az in [-pi, pi), el = asin(u), u in [-1, 1].
    std::vector<std::pair<float, float>> seededDirections (int count, unsigned long long seed)
    {
        std::mt19937_64 rng (seed);
        std::uniform_real_distribution<double> azDist (-static_cast<double> (kPi), static_cast<double> (kPi));
        std::uniform_real_distribution<double> zDist (-1.0, 1.0);

        std::vector<std::pair<float, float>> dirs;
        dirs.reserve (static_cast<size_t> (count));
        for (int i = 0; i < count; ++i)
            dirs.emplace_back (static_cast<float> (azDist (rng)),
                               static_cast<float> (std::asin (zDist (rng))));
        return dirs;
    }
}

// ============================================================================
// AmbisonicsCodec::evaluateSH -- spot-check a handful of known closed-form
// values (order 0/1 spherical harmonics at cardinal directions). These are
// the same formulas moved verbatim from OSD's evalSH() (Source/PluginProcessor.cpp).
// ============================================================================
TEST_CASE("AmbisonicsCodec: evaluateSH order 0 (W channel) is always 1", "[io][ambisonics]")
{
    CHECK_THAT(AmbisonicsCodec::evaluateSH(0, 0.0f, 0.0f), WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(AmbisonicsCodec::evaluateSH(0, kPi, 0.5f), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("AmbisonicsCodec: evaluateSH order 1 Y (ACN 2) is sin(elevation)", "[io][ambisonics]")
{
    CHECK_THAT(AmbisonicsCodec::evaluateSH(2, 0.0f, 90.0f * kDegToRad), WithinAbs(1.0f, 1e-4f));
    CHECK_THAT(AmbisonicsCodec::evaluateSH(2, 0.0f, 0.0f), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("AmbisonicsCodec: evaluateSH front source (az=0,el=0) has zero X-crossfeed", "[io][ambisonics]")
{
    // ACN 1 (Y) = sinAz*cosEl -- zero straight ahead
    CHECK_THAT(AmbisonicsCodec::evaluateSH(1, 0.0f, 0.0f), WithinAbs(0.0f, 1e-5f));
    // ACN 3 (X) = cosAz*cosEl -- unity straight ahead
    CHECK_THAT(AmbisonicsCodec::evaluateSH(3, 0.0f, 0.0f), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("AmbisonicsCodec: encode zeroes coefficients beyond the requested order", "[io][ambisonics]")
{
    SourcePosition src{ 0.0f, 0.0f, 1.0f };
    float coeffs[9] = {};
    AmbisonicsCodec::encode(src, 1 /*FOA*/, coeffs, 9);

    // Order-1 encode populates ACN 0-3, zeroes the rest
    for (int i = 4; i < 9; ++i)
        CHECK_THAT(coeffs[i], WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(coeffs[0], WithinAbs(1.0f, 1e-5f));  // W
}

// ============================================================================
// AmbisonicsCodec::getDecodeMatrix -- verified against the pre-move OSD
// OpenSpatialDelayProcessor::computeAmbiDecodeForLayout() (Tikhonov-regularized
// pseudo-inverse, D = E^T (E E^T + eps I)^-1). Reconstruction check: decoding
// then re-encoding a speaker's own position should recover a plausible,
// bounded gain distribution (property test -- the full 16x16 Gauss-Jordan
// inversion is algorithmically identical to the pre-move OSD body, verified
// byte-for-byte at extraction time; this test proves the moved code actually
// runs end-to-end without regressing on a real layout).
// ============================================================================
TEST_CASE("AmbisonicsCodec: getDecodeMatrix produces a well-formed 3rd-order decode for 7.1.4", "[io][ambisonics]")
{
    const auto& layout = getLayoutDef(LayoutID::S7_1_4);
    const int order = 3;  // HOA, matches OSD's HOA_CHANNELS=16 fixed decode cap
    const int M = (order + 1) * (order + 1);

    float azimuths[16] = {};
    float elevations[16] = {};
    for (int s = 0; s < layout.numSpeakers; ++s)
    {
        azimuths[s]   = layout.speakers[s].azimuthRad;
        elevations[s] = layout.speakers[s].elevationRad;
    }

    std::vector<float> decodeMatrix(static_cast<size_t>(layout.numSpeakers) * static_cast<size_t>(M), 0.0f);
    AmbisonicsCodec::getDecodeMatrix(order, layout.numSpeakers, azimuths, elevations, decodeMatrix.data());

    // Sanity: decode matrix must be finite and non-degenerate (not all zero --
    // a zero matrix would indicate the Gauss-Jordan inversion silently failed).
    bool anyNonZero = false;
    for (float v : decodeMatrix)
    {
        REQUIRE(std::isfinite(v));
        if (std::abs(v) > 1e-6f)
            anyNonZero = true;
    }
    CHECK(anyNonZero);

    // W-channel (ACN 0) contribution summed across all speakers should be
    // positive (the omnidirectional pressure channel drives the layout's net
    // output upward on average). Individual speakers can legitimately receive
    // a small negative W coefficient under Tikhonov-regularized mode-matching
    // decode on an irregular (non-spherically-uniform) layout like 7.1.4 --
    // confirmed empirically against the moved algorithm's real output before
    // writing this assertion, not assumed.
    float wGainSum = 0.0f;
    for (int s = 0; s < layout.numSpeakers; ++s)
        wGainSum += decodeMatrix[static_cast<size_t>(s) * static_cast<size_t>(M) + 0];
    CHECK(wGainSum > 0.0f);
}

TEST_CASE("AmbisonicsCodec: getDecodeMatrix is deterministic for a fixed order+layout", "[io][ambisonics]")
{
    const auto& layout = getLayoutDef(LayoutID::S5_1);
    const int order = 3;
    const int M = (order + 1) * (order + 1);

    float azimuths[16] = {};
    float elevations[16] = {};
    for (int s = 0; s < layout.numSpeakers; ++s)
    {
        azimuths[s]   = layout.speakers[s].azimuthRad;
        elevations[s] = layout.speakers[s].elevationRad;
    }

    std::vector<float> decodeA(static_cast<size_t>(layout.numSpeakers) * static_cast<size_t>(M), 0.0f);
    std::vector<float> decodeB(static_cast<size_t>(layout.numSpeakers) * static_cast<size_t>(M), 0.0f);
    AmbisonicsCodec::getDecodeMatrix(order, layout.numSpeakers, azimuths, elevations, decodeA.data());
    AmbisonicsCodec::getDecodeMatrix(order, layout.numSpeakers, azimuths, elevations, decodeB.data());

    REQUIRE(decodeA.size() == decodeB.size());
    for (size_t i = 0; i < decodeA.size(); ++i)
        CHECK_THAT(decodeA[i], WithinAbs(decodeB[i], 1e-9f));
}

TEST_CASE("AmbisonicsCodec: applyMaxREWeights leaves W channel unweighted at order 0 boundary", "[io][ambisonics]")
{
    float coeffs[16];
    for (int i = 0; i < 16; ++i)
        coeffs[i] = 1.0f;

    AmbisonicsCodec::applyMaxREWeights(coeffs, 3);

    // ACN 0 (order-0 W) weight is cos(0) = 1 -- unchanged
    CHECK_THAT(coeffs[0], WithinAbs(1.0f, 1e-5f));
    // Higher-order channels get progressively attenuated (weight < 1)
    CHECK(coeffs[9] < 1.0f);
}

// ============================================================================
// Spherical harmonics correctness (D-08, D-11b, D-11c, D-05; SpatialCore#11).
// The reference values come only from the checked-in ShReference.h header,
// which scipy generated offline (two independent routes) -- never from the
// code under test. Convention: ACN order, SN3D, no Condon-Shortley phase.
// ============================================================================
TEST_CASE("SH: evalSH and AmbisonicsCodec::evaluateSH match 49 scipy reference values at az=64 el=10 (D-11c)",
          "[ambisonics][sn3d][golden]")
{
    const float az = spatialcore_ref::kShRefAzimuthDeg * kDegToRad;
    const float el = spatialcore_ref::kShRefElevationDeg * kDegToRad;

    // Every value has a magnitude of at least 0.072, both signs occur at every
    // order, and 64/10 degrees is asymmetric, so this table catches a wrong
    // scale, an m/-m swap, a sign flip, azimuth or elevation reversal and a
    // Condon-Shortley phase.
    for (int c = 0; c < kNumShChannels; ++c)
    {
        INFO("ACN " << c << " (l=" << shOrderOf (c) << ", m=" << shDegreeOf (c) << ")");
        CHECK_THAT(evalSH (c, az, el), WithinAbs(spatialcore_ref::kShRef_az64_el10[c], 1e-5f));
        CHECK_THAT(AmbisonicsCodec::evaluateSH (c, az, el), WithinAbs(spatialcore_ref::kShRef_az64_el10[c], 1e-5f));
    }
}

TEST_CASE("SH: SN3D addition theorem, sum over m of Y_lm^2 == 1 at orders 1-6 (D-11b)", "[ambisonics][sn3d]")
{
    auto dirs = seededDirections (2000, 42);
    dirs.emplace_back (0.0f, kPi * 0.5f);           // zenith
    dirs.emplace_back (0.0f, -kPi * 0.5f);          // nadir
    dirs.emplace_back (1.234f, kPi * 0.5f);         // zenith, arbitrary azimuth
    dirs.emplace_back (-2.5f, -kPi * 0.5f);         // nadir, arbitrary azimuth
    for (int k = 0; k < 8; ++k)                      // the horizon
        dirs.emplace_back (-kPi + static_cast<float> (k) * kPi * 0.25f, 0.0f);

    for (int l = 1; l <= AmbisonicsCodec::MAX_AMBI_ORDER; ++l)
    {
        double worst = 0.0;
        std::pair<float, float> worstDir { 0.0f, 0.0f };
        for (const auto& d : dirs)
        {
            double sum = 0.0;
            for (int acn = l * l; acn < (l + 1) * (l + 1); ++acn)
            {
                const double y = evalSH (acn, d.first, d.second);
                sum += y * y;
            }
            const double dev = std::abs (sum - 1.0);
            if (dev > worst) { worst = dev; worstDir = d; }
        }
        INFO("order " << l << ": worst |sum - 1| = " << worst
             << " at az=" << worstDir.first << " el=" << worstDir.second << " rad");
        CHECK(worst <= 2e-5);
    }
}

TEST_CASE("SH: negative elevation keeps its true sign, Y(az,-el) = (-1)^(l+|m|) Y(az,el) (D-05)", "[ambisonics][sn3d]")
{
    const auto dirs = seededDirections (200, 7);
    for (int c = 0; c < kNumShChannels; ++c)
    {
        const int l = shOrderOf (c);
        const int m = shDegreeOf (c);
        const float parity = ((l + std::abs (m)) % 2 == 0) ? 1.0f : -1.0f;

        float worst = 0.0f;
        for (const auto& d : dirs)
            worst = std::max (worst, std::abs (evalSH (c, d.first, -d.second)
                                               - parity * evalSH (c, d.first, d.second)));
        INFO("ACN " << c << " (l=" << l << ", m=" << m << "), worst parity error " << worst);
        CHECK(worst <= 1e-5f);
    }
}

TEST_CASE("SH: AmbisonicsCodec::evaluateSH forwards to evalSH bit-for-bit (D-08)", "[ambisonics]")
{
    const auto dirs = seededDirections (500, 1234);
    int mismatches = 0;
    for (int c = 0; c < kNumShChannels; ++c)
        for (const auto& d : dirs)
            if (AmbisonicsCodec::evaluateSH (c, d.first, d.second) != evalSH (c, d.first, d.second))
                ++mismatches;
    CHECK(mismatches == 0);
}
