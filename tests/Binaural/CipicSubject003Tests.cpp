#include <catch2/catch_test_macros.hpp>
#include "BinauralTestUtilities.h"

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// CIPIC Subject 003 ("Natural", OSD profile 2) — own control, own golden.
// See SadieD2KU100Tests.cpp for the locked-decision rationale (CONTEXT.md):
// each profile is tested individually, never via a shared representative.
// ============================================================================

namespace
{
    constexpr const char* kSofaFile = "cipic_subject_003.sofa";

    // Golden fingerprint captured via HRTFDatabase::getAlignedHRIR() at az=90deg
    // (hard left), el=0deg — same path/azimuth as every other profile in this
    // suite, so cross-profile comparisons of the printed delay values remain
    // meaningful even though fingerprints are never compared across profiles.
    // Tolerance fingerprint of the az=90deg ITD-aligned HRIR pair (field order: irLength,
    // delayL, delayR, energyL, energyR, peakL, peakR, peakIndexL, peakIndexR).
    // Captured from this tree at commit 12d6929 on 2026-10-04 with libmysofa v1.3.2.
    // Replaces the FNV checksum 0xec93148ea0ba8fe8: byte-exact float hashes differ across
    // build types by one ulp (research Pitfall 6). D-17 governs any future change of these
    // values.
    constexpr HRIRFingerprint kGoldenFingerprint { 218, 25.0f, 54.0f, 8.71853987, 0.107548025, 1.64910412f, -0.127475545f, 26, 42 };
    constexpr int kGoldenIRLength = 218;
    constexpr int kGoldenNumPositions = 1250;
}

TEST_CASE ("CIPIC Subject 003 — synchronous load succeeds", "[binaural][cipic][sync-load]")
{
    HRTFDatabase db;
    REQUIRE_FALSE (db.isLoaded());

    bool ok = db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate));

    REQUIRE (ok);
    REQUIRE (db.isLoaded());
    CHECK (db.getIRLength() == kGoldenIRLength);
    CHECK (db.getNumPositions() == kGoldenNumPositions);
}

TEST_CASE ("CIPIC Subject 003 — golden HRIR fingerprint at az=90deg (own control)", "[binaural][cipic][golden]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate)));

    std::vector<float> irL (static_cast<size_t> (db.getIRLength()));
    std::vector<float> irR (static_cast<size_t> (db.getIRLength()));
    float delayL = 0.0f, delayR = 0.0f;

    db.getAlignedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), delayL, delayR);

    const HRIRFingerprint actual = fingerprintAlignedHRIR (db, 90.0f, 0.0f);
    std::string why;
    const bool matches = fingerprintMatches (actual, kGoldenFingerprint, &why);
    INFO (why);
    CHECK (matches);

    // Hard-left source: left ear (near side) should report a smaller ITD
    // magnitude than the right ear (far side) for a physically plausible head model.
    CHECK (std::abs (delayL) < std::abs (delayR));

    SECTION ("negative control: a 0.01% gain change fails the fingerprint")
    {
        std::vector<float> scaled (irL);
        for (auto& x : scaled)
            x *= 1.0001f;
        const auto fp = fingerprintHRIRPair (scaled.data(), irR.data(), db.getIRLength(), delayL, delayR);
        CHECK_FALSE (fingerprintMatches (fp, kGoldenFingerprint));
    }

    SECTION ("negative control: a one-sample shift fails the fingerprint")
    {
        std::vector<float> shifted (irL.size(), 0.0f);
        std::copy (irL.begin(), irL.end() - 1, shifted.begin() + 1);
        const auto fp = fingerprintHRIRPair (shifted.data(), irR.data(), db.getIRLength(), delayL, delayR);
        CHECK_FALSE (fingerprintMatches (fp, kGoldenFingerprint));
    }
}

TEST_CASE ("CIPIC Subject 003 — negative control: unloaded database has no data", "[binaural][cipic][negative-control]")
{
    HRTFDatabase db;
    CHECK_FALSE (db.isLoaded());
    CHECK (db.getIRLength() == 0);
    CHECK (db.getNumPositions() == 0);

    std::vector<float> irL (16, -1.0f), irR (16, -1.0f);
    float delayL = 0.0f, delayR = 0.0f;
    db.getInterpolatedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), delayL, delayR);

    CHECK (irL[0] == -1.0f);
    CHECK (irR[0] == -1.0f);
}
