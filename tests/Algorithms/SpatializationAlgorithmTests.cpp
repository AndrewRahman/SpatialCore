#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include "../reference/PanningReference.h"
#include <memory>
#include <cmath>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Fresh, deep per-algorithm test suite (Phase 8 Plan 08-03, D-09 — replaces
// the prior smoke-only coverage with per-algorithm golden-vector assertions).
//
// Fixed test layout: Quad (matches SpatialCore::Layouts::getQuad()) — 4
// speakers at azimuths 45, -45, 135, -135 degrees, all at 0 elevation.
// Golden vectors below were computed independently against the pre-move OSD
// algorithm bodies (byte-identical formulas, verified in 08-03-PLAN.md Task 1)
// for this fixed layout + fixed source inputs, proving byte-identical
// extraction rather than merely plausible output (D-09).
// ============================================================================

namespace
{
    SpeakerLayout makeQuadLayout()
    {
        SpeakerLayout layout {};
        layout.numSpeakers     = 4;
        layout.lfeChannelIndex = -1;
        layout.totalChannels   = 4;
        const float az[] = { 45.0f, -45.0f, 135.0f, -135.0f };
        for (int i = 0; i < 4; ++i)
        {
            layout.speakers[i].azimuthRad   = juce::degreesToRadians (az[i]);
            layout.speakers[i].elevationRad = 0.0f;
            layout.speakers[i].channelIndex = i;
        }
        return layout;
    }

    SourcePosition makeSource (float azDeg, float elDeg, float distance = 0.5f)
    {
        return { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), distance };
    }

    LayoutContext makeCtx (const SpeakerLayout& layout,
                           const std::vector<VBAPTriplet>& triplets,
                           const float (*ambiMatrix)[MAX_SPEAKERS],
                           int ambiNumSpeakers)
    {
        return { layout, triplets, ambiMatrix, ambiNumSpeakers };
    }
}

// ============================================================================
// Instantiation / interface-contract smoke coverage (carried from the prior
// suite, kept as a fast sanity net underneath the deeper tests below)
// ============================================================================

TEST_CASE ("All 8 algorithms instantiate and have names", "[algorithms]")
{
    std::unique_ptr<SpatializationAlgorithm> algos[] = {
        std::make_unique<VBAPAlgorithm>(),
        std::make_unique<VBIPAlgorithm>(),
        std::make_unique<KNNAlgorithm>(),
        std::make_unique<DBAPAlgorithm>(),
        std::make_unique<MDAPAlgorithm>(),
        std::make_unique<AmbisonicsAlgorithm>(),
        std::make_unique<ConstantPowerAlgorithm>(),
        std::make_unique<DirectBinauralAlgorithm>(),
    };

    for (auto& algo : algos)
        REQUIRE (algo->getName().isNotEmpty());
}

TEST_CASE ("DirectBinaural supports binaural direct, not surround", "[algorithms][directbinaural]")
{
    DirectBinauralAlgorithm algo;
    REQUIRE (algo.supportsBinauralDirect() == true);
    REQUIRE (algo.supportsSurround() == false);
    REQUIRE (algo.supportsSHDomain() == false);
}

TEST_CASE ("Ambisonics supports SH domain only", "[algorithms][ambisonics]")
{
    AmbisonicsAlgorithm algo;
    REQUIRE (algo.supportsSHDomain() == true);
    REQUIRE (algo.supportsBinauralDirect() == false);
    REQUIRE (algo.supportsSurround() == true);
}

TEST_CASE ("VBAP/VBIP/KNN/DBAP/MDAP/ConstantPower support surround, not binaural direct or SH", "[algorithms]")
{
    std::unique_ptr<SpatializationAlgorithm> algos[] = {
        std::make_unique<VBAPAlgorithm>(),
        std::make_unique<VBIPAlgorithm>(),
        std::make_unique<KNNAlgorithm>(),
        std::make_unique<DBAPAlgorithm>(),
        std::make_unique<MDAPAlgorithm>(),
        std::make_unique<ConstantPowerAlgorithm>(),
    };
    for (auto& algo : algos)
    {
        REQUIRE (algo->supportsBinauralDirect() == false);
        REQUIRE (algo->supportsSurround() == true);
        REQUIRE (algo->supportsSHDomain() == false);
    }
}

// ============================================================================
// Test 1 (locked baseline control path): ConstantPower / Equal Power stereo
// panning — the "most basic encoding" sanity reference before binaural
// complexity (08-CONTEXT.md locked decision). Hard-left / center / hard-right
// against the Quad layout's front pair (speakers at +45/-45 deg).
// ============================================================================

TEST_CASE ("ConstantPower: hard-left source (on-axis with +45 speaker) gives that speaker full gain",
           "[algorithms][constantpower][baseline]")
{
    ConstantPowerAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    algo.computeGains (makeSource (45.0f, 0.0f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (1.0f, 1e-5f));
    CHECK_THAT (gains[1], WithinAbs (0.0f, 1e-4f));
    CHECK_THAT (gains[2], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("ConstantPower: hard-right source (on-axis with -45 speaker) gives that speaker full gain",
           "[algorithms][constantpower][baseline]")
{
    ConstantPowerAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    algo.computeGains (makeSource (-45.0f, 0.0f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (0.0f, 1e-4f));
    CHECK_THAT (gains[1], WithinAbs (1.0f, 1e-5f));
    CHECK_THAT (gains[2], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("ConstantPower: center source produces the equal-power cos/sin split (locked baseline law)",
           "[algorithms][constantpower][baseline]")
{
    ConstantPowerAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    algo.computeGains (makeSource (0.0f, 0.0f), ctx, gains, 4);

    // Source at az=0 sits exactly between the +45/-45 front pair — cos(45deg)
    // == sin(45deg) == 0.70710678, the textbook equal-power panning law.
    static constexpr float kCosSin45 = 0.70710678f;
    CHECK_THAT (gains[0], WithinAbs (kCosSin45, 1e-5f));
    CHECK_THAT (gains[1], WithinAbs (kCosSin45, 1e-5f));
    CHECK_THAT (gains[2], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));

    // Constant-power invariant: sum of squared gains == 1
    float power = gains[0]*gains[0] + gains[1]*gains[1] + gains[2]*gains[2] + gains[3]*gains[3];
    CHECK_THAT (power, WithinAbs (1.0f, 1e-5f));
}

// ============================================================================
// Test 2: Golden-vector assertions per remaining algorithm (fixed source,
// fixed Quad layout) — proves byte-identical extraction, not just plausible
// output. All values below computed against the pre-move OSD algorithm
// formulas for source (az=30 deg, el=0 deg, distance=0.5).
// ============================================================================

TEST_CASE ("VBAP: golden gain vector at az=30 on Quad layout", "[algorithms][vbap][golden]")
{
    VBAPAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;   // empty -- Quad has no height, forces 2D path
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = {};
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (0.9659258f, 1e-5f));
    CHECK_THAT (gains[1], WithinAbs (0.2588190f, 1e-5f));
    CHECK_THAT (gains[2], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("VBIP: textbook gain vector at az=30 on Quad layout (Pernaux, Boussard & Jot, DAFx-98)",
           "[algorithms][vbip][golden]")
{
    // Expected values come from tests/reference/PanningReference.h, generated from the
    // published VBIP formula g_i = sqrt(G_i / sum G), G = L^-1 p (D-14) -- not from this code.
    VBIPAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = {};
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    for (int s = 0; s < 4; ++s)
        CHECK_THAT (gains[s], WithinAbs (spatialcore_ref::kVbip_Quad_az30[s], 1e-5f));
}

TEST_CASE ("VBIP: unit power and energy vector aimed at the source across 2D sweeps (D-14)", "[algorithms][vbip]")
{
    // Textbook VBIP (DAFx-98 sec. 2.2.2) aims the energy vector rE = sum g^2 l at the source
    // and keeps sum g^2 = 1 everywhere, on every flat layout.
    const LayoutID ids[] = { Quad, S5_0, S7_0, S9_1, Octaphonic };
    VBIPAlgorithm algo;
    std::vector<VBAPTriplet> triplets;   // flat layouts: 2D pair path
    float ambiMatrix[1][MAX_SPEAKERS] = {};

    for (LayoutID id : ids)
    {
        const SpeakerLayout& layout = getLayoutDef (id);
        LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);
        const int n = layout.numSpeakers;

        for (int k = 0; k < 720; ++k)
        {
            const float azDeg = -179.63f + 0.5f * static_cast<float> (k);
            float gains[MAX_SPEAKERS] = {};
            algo.computeGains (makeSource (azDeg, 0.0f), ctx, gains, n);

            double power = 0.0, ex = 0.0, ey = 0.0;
            for (int s = 0; s < n; ++s)
            {
                const double g2 = static_cast<double> (gains[s]) * gains[s];
                power += g2;
                ex += g2 * std::sin (static_cast<double> (layout.speakers[s].azimuthRad));
                ey += g2 * std::cos (static_cast<double> (layout.speakers[s].azimuthRad));
            }

            INFO ("layout " << static_cast<int> (id) << " az " << azDeg);
            CHECK_THAT (power, WithinAbs (1.0, 1e-5));

            const double rEdeg = std::atan2 (ex, ey) * 180.0 / juce::MathConstants<double>::pi;
            const double diff  = std::remainder (rEdeg - static_cast<double> (azDeg), 360.0);
            CHECK_THAT (diff, WithinAbs (0.0, 1e-3));
        }
    }
}

TEST_CASE ("VBIP: wider than VBAP between two speakers (D-14)", "[algorithms][vbip]")
{
    VBIPAlgorithm vbip;
    VBAPAlgorithm vbap;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float vbipGains[4] = {};
    float vbapGains[4] = {};
    vbip.computeGains (makeSource (30.0f, 0.0f), ctx, vbipGains, 4);
    vbap.computeGains (makeSource (30.0f, 0.0f), ctx, vbapGains, 4);

    // Speaker 1 (-45) is the far member of the active pair at az 30.
    CHECK (vbipGains[1] > vbapGains[1]);
    CHECK (vbipGains[0] < vbapGains[0]);
}

TEST_CASE ("KNN: golden gain vector at az=30 on Quad layout (k=3, inverse-distance-squared)",
           "[algorithms][knn][golden]")
{
    KNNAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = {};
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (0.9989932f, 1e-4f));
    CHECK_THAT (gains[1], WithinAbs (0.0399603f, 1e-4f));
    CHECK_THAT (gains[2], WithinAbs (0.0203879f, 1e-4f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("DBAP: golden gain vector at az=30, dist=0.5 on Quad layout (Euclidean, inverse-dist-squared)",
           "[algorithms][dbap][golden]")
{
    DBAPAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = {};
    algo.computeGains (makeSource (30.0f, 0.0f, 0.5f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (0.9390509f, 1e-4f));
    CHECK_THAT (gains[1], WithinAbs (0.2691336f, 1e-4f));
    CHECK_THAT (gains[2], WithinAbs (0.1768006f, 1e-4f));
    CHECK_THAT (gains[3], WithinAbs (0.1203831f, 1e-4f));

    // These values equal Lossius et al., ICMC 2009 eq. 2-5 evaluated at SpatialCore's
    // effective rolloff R = 12.04 dB (a = 2, 1/d^2 used as amplitude, no blur), cross-checked
    // by tests/reference/gen_panning_reference.py (RESEARCH F2) -- a textbook check, not
    // merely a regression pin.
    for (int s = 0; s < 4; ++s)
        CHECK_THAT (gains[s], WithinAbs (spatialcore_ref::kDbap_Quad_az30_dist05[s], 1e-5f));

    // DBAP always activates every speaker (weighted by inverse Euclidean
    // distance) -- unlike VBAP, there is no hard zero-gain speaker here.
    for (float g : gains)
        CHECK (g > 0.0f);
}

// ============================================================================
// Test 3: MDAP and Ambisonics — property-based assertions (spread/normalization
// invariants) rather than hand-derived golden vectors, since both algorithms'
// formulas (9-point VBAP ring sum with Rodrigues rotation; full 16x16 SH
// decode matrix) are impractical to hand-verify independently without
// re-implementing the production code. These still prove the extracted
// bodies preserve the documented mathematical invariants.
// ============================================================================

TEST_CASE ("MDAP: produces wider spread than point VBAP for the same source (Pulkki, WASPAA 1999 spread ring)",
           "[algorithms][mdap]")
{
    MDAPAlgorithm mdap;
    VBAPAlgorithm vbap;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float mdapGains[4] = {};
    float vbapGains[4] = {};
    mdap.computeGains (makeSource (30.0f, 0.0f), ctx, mdapGains, 4);
    vbap.computeGains (makeSource (30.0f, 0.0f), ctx, vbapGains, 4);

    // MDAP should activate at least one speaker beyond point VBAP's active
    // pair (from the 30-degree spread ring), unlike point VBAP which zeroes
    // every speaker outside the immediate +-45 pair at az=30. The ring's
    // radius (30 deg, per the alphaDegs formula for a 4-speaker layout) only
    // reaches as far as the near-side rear speaker (135 deg); the far-side
    // rear speaker (-135 deg) is outside the ring's reach and stays at zero.
    CHECK (vbapGains[2] == 0.0f);
    CHECK (vbapGains[3] == 0.0f);
    CHECK (mdapGains[2] > 0.0f);
    CHECK (mdapGains[3] == 0.0f);

    // Constant-power (energy) normalization invariant: sum of squares == 1
    float power = 0.0f;
    for (float g : mdapGains) power += g * g;
    CHECK_THAT (power, WithinAbs (1.0f, 1e-4f));
}

TEST_CASE ("MDAP: symmetric sources produce mirror-symmetric gains", "[algorithms][mdap]")
{
    MDAPAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float leftGains[4] = {};
    float rightGains[4] = {};
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, leftGains, 4);
    algo.computeGains (makeSource (-30.0f, 0.0f), ctx, rightGains, 4);

    // Quad layout is mirror-symmetric about the front/back axis (speakers at
    // +-45, +-135) so az=+30 and az=-30 should swap L<->R and Ls<->Rs gains.
    CHECK_THAT (leftGains[0], WithinAbs (rightGains[1], 1e-4f));
    CHECK_THAT (leftGains[1], WithinAbs (rightGains[0], 1e-4f));
    CHECK_THAT (leftGains[2], WithinAbs (rightGains[3], 1e-4f));
    CHECK_THAT (leftGains[3], WithinAbs (rightGains[2], 1e-4f));
}

TEST_CASE ("Ambisonics: zero decode matrix produces all-zero gains (no spurious energy)",
           "[algorithms][ambisonics]")
{
    AmbisonicsAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[4][MAX_SPEAKERS] = {};   // all zero
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 4);

    float gains[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    for (float g : gains)
        CHECK_THAT (g, WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("Ambisonics: identity-like single-channel decode routes W-only energy to one speaker",
           "[algorithms][ambisonics]")
{
    AmbisonicsAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[4][MAX_SPEAKERS] = {};
    // ACN 0 (W, omnidirectional) drives speaker 0 only, all other channels/speakers zero.
    ambiMatrix[0][0] = 1.0f;
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 4);

    float gains[4] = {};
    // Source direction is irrelevant to the W channel (evalSH(0,...) == 1 always),
    // so any source position should drive speaker 0 to full gain (post constant-power norm).
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (1.0f, 1e-4f));
    CHECK_THAT (gains[1], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[2], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));
}

// ============================================================================
// DirectBinauralAlgorithm — Woodworth ITD/ILD model (used internally for the
// "Simple (Low CPU)" profile). computeGains() is a speaker-domain no-op
// (zeros); computeBinauralGains() is the real model under test here.
// ============================================================================

TEST_CASE ("DirectBinaural: computeGains zeroes all speaker outputs (binaural-only algorithm)",
           "[algorithms][directbinaural]")
{
    DirectBinauralAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    for (float g : gains)
        CHECK_THAT (g, WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("DirectBinaural: center source (az=0) produces equal, undelayed L/R gains",
           "[algorithms][directbinaural][golden]")
{
    DirectBinauralAlgorithm algo;

    BinauralProfile profile { 0.0875f, 1.0f, 3000.0f, "Test Profile" };
    BinauralContext ctx { 1, 48000.0, &profile };

    auto gains = algo.computeBinauralGains (makeSource (0.0f, 0.0f, 0.5f), ctx);

    // At az=0, lateral = sin(0)*cos(0) = 0 -- symmetric, no ITD, no ILD.
    CHECK_THAT (gains.leftGain, WithinAbs (gains.rightGain, 1e-6f));
    CHECK_THAT (gains.leftDelaySamples, WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains.rightDelaySamples, WithinAbs (0.0f, 1e-6f));

    // Distance attenuation golden value: 1 / max(0.1, 0.5*4+0.25) = 1/2.25
    static constexpr float kExpectedDistGain = 1.0f / 2.25f;
    CHECK_THAT (gains.leftGain, WithinAbs (kExpectedDistGain, 1e-5f));
}

TEST_CASE ("DirectBinaural: hard-left source (az=90) delays and attenuates the right (far) ear",
           "[algorithms][directbinaural][golden]")
{
    DirectBinauralAlgorithm algo;

    BinauralProfile profile { 0.0875f, 1.0f, 3000.0f, "Test Profile" };
    BinauralContext ctx { 1, 48000.0, &profile };

    auto gains = algo.computeBinauralGains (makeSource (90.0f, 0.0f, 0.5f), ctx);

    // Source on the LEFT (positive azimuth = left per ADM): left ear is near,
    // right ear is far -- right gets attenuated (ILD) and delayed (ITD).
    CHECK (gains.rightGain < gains.leftGain);
    CHECK (gains.rightDelaySamples > 0.0f);
    CHECK_THAT (gains.leftDelaySamples, WithinAbs (0.0f, 1e-6f));
}
