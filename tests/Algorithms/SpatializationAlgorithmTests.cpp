#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include <SpatialCore/IO/SpeakerLayout.h>
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

TEST_CASE ("VBIP: golden gain vector at az=30 on Quad layout (squared VBAP, renormalized)",
           "[algorithms][vbip][golden]")
{
    VBIPAlgorithm algo;
    SpeakerLayout layout = makeQuadLayout();
    std::vector<VBAPTriplet> triplets;
    float ambiMatrix[1][MAX_SPEAKERS] = {};
    LayoutContext ctx = makeCtx (layout, triplets, ambiMatrix, 0);

    float gains[4] = {};
    algo.computeGains (makeSource (30.0f, 0.0f), ctx, gains, 4);

    CHECK_THAT (gains[0], WithinAbs (0.9330127f, 1e-5f));
    CHECK_THAT (gains[1], WithinAbs (0.0669873f, 1e-5f));
    CHECK_THAT (gains[2], WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains[3], WithinAbs (0.0f, 1e-6f));
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

TEST_CASE ("MDAP: produces wider spread than point VBAP for the same source (Pulkki 2000 spread ring)",
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
