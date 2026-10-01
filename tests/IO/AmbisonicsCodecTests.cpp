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

// ============================================================================
// getDecodeMatrix bounds (D-20, RESEARCH F6). E is sized MAX_SPEAKERS columns,
// so more than 16 speakers used to write past a stack array. The guard makes
// it write nothing; 0 speakers also writes nothing; exactly 16 decodes.
// ============================================================================
TEST_CASE("AmbisonicsCodec: getDecodeMatrix writes nothing for more than 16 or for 0 speakers (D-20)",
          "[ambisonics][decode-guard]")
{
    constexpr int order = 3;
    constexpr int M = (order + 1) * (order + 1);
    constexpr float kSentinel = 12345.0f;
    constexpr int kTooMany = MAX_SPEAKERS + 1;   // 17

    float az[kTooMany] = {};
    float el[kTooMany] = {};
    for (int s = 0; s < kTooMany; ++s)
    {
        // Distinct directions spread over the upper and lower hemisphere.
        az[s] = -kPi + (2.0f * kPi) * static_cast<float> (s) / static_cast<float> (kTooMany);
        el[s] = std::asin (1.0f - (2.0f * static_cast<float> (s) + 1.0f) / static_cast<float> (kTooMany));
    }

    std::vector<float> out (static_cast<size_t> (kTooMany) * static_cast<size_t> (M), kSentinel);

    SECTION("17 speakers: the whole 17 x 16 buffer is untouched")
    {
        AmbisonicsCodec::getDecodeMatrix (order, kTooMany, az, el, out.data());
        int changed = 0;
        for (float v : out)
            if (v != kSentinel)
                ++changed;
        CHECK(changed == 0);
    }

    SECTION("0 speakers: the buffer is untouched")
    {
        AmbisonicsCodec::getDecodeMatrix (order, 0, az, el, out.data());
        int changed = 0;
        for (float v : out)
            if (v != kSentinel)
                ++changed;
        CHECK(changed == 0);
    }

    SECTION("16 speakers (the boundary): every one of the 16 x 16 entries is written and finite")
    {
        AmbisonicsCodec::getDecodeMatrix (order, MAX_SPEAKERS, az, el, out.data());
        int written = 0, finite = 0;
        for (int i = 0; i < MAX_SPEAKERS * M; ++i)
        {
            if (out[static_cast<size_t> (i)] != kSentinel) ++written;
            if (std::isfinite (out[static_cast<size_t> (i)])) ++finite;
        }
        CHECK(written == MAX_SPEAKERS * M);
        CHECK(finite == MAX_SPEAKERS * M);
        // Row 17 (index 16) lies beyond numSpeakers and must stay untouched.
        for (int c = 0; c < M; ++c)
            CHECK(out[static_cast<size_t> (MAX_SPEAKERS * M + c)] == kSentinel);
    }
}

// ============================================================================
// Encode -> dense decode -> re-encode round trip (D-11a) -- a DECODER-
// CONDITIONING SMOKE TEST ONLY, not evidence of SH correctness.
//
// D-12: a mode-matching round trip is blind to per-channel scale errors.
// If the evaluator is off by a per-channel scale S (Y' = S Y), the decoder
// built from it is pinv(S Y) = pinv(Y) S^-1, and the re-encode multiplies S
// back in, so S cancels exactly. This test passed identically on the pre-fix
// constants at every order (RESEARCH F6: 1.5e-8 .. 6.2e-8, buggy == fixed).
// SH correctness is proven by the [sn3d] literal and addition-theorem tests.
//
// The decoder is test-local (200-point Fibonacci lattice, double precision,
// Tikhonov epsilon 1e-6), because the shipped getDecodeMatrix caps at 16
// speakers and is rank-limited above order 1 (F6); it says nothing about the
// shipped decoder, which [ambi-pin] covers.
// ============================================================================
namespace
{
    // D = E^T (E E^T + eps I)^-1, the same shape as getDecodeMatrix, in double.
    // E is M x S (row c = channel, column s = speaker); returns D as S x M.
    std::vector<double> denseDecode (const std::vector<double>& E, int M, int S, double eps)
    {
        std::vector<double> aug (static_cast<size_t> (M * M), 0.0);
        std::vector<double> inv (static_cast<size_t> (M * M), 0.0);
        auto at = [M] (std::vector<double>& m, int r, int c) -> double& { return m[static_cast<size_t> (r * M + c)]; };

        for (int i = 0; i < M; ++i)
        {
            for (int j = 0; j < M; ++j)
            {
                double sum = 0.0;
                for (int s = 0; s < S; ++s)
                    sum += E[static_cast<size_t> (i * S + s)] * E[static_cast<size_t> (j * S + s)];
                at (aug, i, j) = sum;
            }
            at (aug, i, i) += eps;
            at (inv, i, i) = 1.0;
        }

        // Gauss-Jordan with partial pivoting.
        for (int col = 0; col < M; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < M; ++row)
                if (std::abs (at (aug, row, col)) > std::abs (at (aug, pivot, col)))
                    pivot = row;
            if (pivot != col)
                for (int j = 0; j < M; ++j)
                {
                    std::swap (at (aug, col, j), at (aug, pivot, j));
                    std::swap (at (inv, col, j), at (inv, pivot, j));
                }

            const double diag = at (aug, col, col);
            for (int j = 0; j < M; ++j)
            {
                at (aug, col, j) /= diag;
                at (inv, col, j) /= diag;
            }
            for (int row = 0; row < M; ++row)
            {
                if (row == col) continue;
                const double f = at (aug, row, col);
                for (int j = 0; j < M; ++j)
                {
                    at (aug, row, j) -= f * at (aug, col, j);
                    at (inv, row, j) -= f * at (inv, col, j);
                }
            }
        }

        std::vector<double> D (static_cast<size_t> (S * M), 0.0);
        for (int s = 0; s < S; ++s)
            for (int c = 0; c < M; ++c)
            {
                double sum = 0.0;
                for (int k = 0; k < M; ++k)
                    sum += E[static_cast<size_t> (k * S + s)] * at (inv, k, c);
                D[static_cast<size_t> (s * M + c)] = sum;
            }
        return D;
    }
}

TEST_CASE("SH: encode, dense decode, re-encode round trip at orders 1-6 (D-11a, smoke test only)",
          "[ambisonics][roundtrip]")
{
    // 200-point Fibonacci lattice: near-uniform sphere sampling, no table needed.
    constexpr int S = 200;
    const double goldenAngle = 3.14159265358979323846 * (3.0 - std::sqrt (5.0));
    std::vector<float> latAz (S), latEl (S);
    for (int i = 0; i < S; ++i)
    {
        const double z = 1.0 - (2.0 * i + 1.0) / S;
        latEl[static_cast<size_t> (i)] = static_cast<float> (std::asin (z));
        latAz[static_cast<size_t> (i)] = static_cast<float> (std::remainder (goldenAngle * i, 2.0 * 3.14159265358979323846));
    }

    const auto sources = seededDirections (50, 99);

    for (int order = 1; order <= AmbisonicsCodec::MAX_AMBI_ORDER; ++order)
    {
        const int M = (order + 1) * (order + 1);

        std::vector<double> E (static_cast<size_t> (M * S));
        for (int c = 0; c < M; ++c)
            for (int s = 0; s < S; ++s)
                E[static_cast<size_t> (c * S + s)] = AmbisonicsCodec::evaluateSH (c, latAz[static_cast<size_t> (s)],
                                                                                  latEl[static_cast<size_t> (s)]);
        const auto D = denseDecode (E, M, S, 1e-6);

        double worst = 0.0;
        for (const auto& src : sources)
        {
            float coeffs[AmbisonicsCodec::MAX_AMBI_CHANNELS] = {};
            AmbisonicsCodec::encode (SourcePosition { src.first, src.second, 1.0f }, order, coeffs, M);

            std::vector<double> g (static_cast<size_t> (S), 0.0);       // speaker gains = D y
            for (int s = 0; s < S; ++s)
                for (int c = 0; c < M; ++c)
                    g[static_cast<size_t> (s)] += D[static_cast<size_t> (s * M + c)] * coeffs[c];

            for (int c = 0; c < M; ++c)                                // re-encode = E g
            {
                double y = 0.0;
                for (int s = 0; s < S; ++s)
                    y += E[static_cast<size_t> (c * S + s)] * g[static_cast<size_t> (s)];
                worst = std::max (worst, std::abs (y - static_cast<double> (coeffs[c])));
            }
        }
        INFO("order " << order << ": worst re-encode coefficient error " << worst);
        CHECK(worst <= 1e-5);
    }
}
