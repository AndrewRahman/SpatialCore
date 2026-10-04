#include <catch2/catch_test_macros.hpp>
#include "BinauralTestUtilities.h"

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// Bernschuetz KU100 ("Spatial", OSD profile 4) — own control, own golden.
// See SadieD2KU100Tests.cpp for the locked-decision rationale (CONTEXT.md):
// each profile is tested individually, never via a shared representative.
// ============================================================================

namespace
{
    constexpr const char* kSofaFile = "bernschuetz_ku100.sofa";

    // Tolerance fingerprint of the az=90deg ITD-aligned HRIR pair (field order: irLength,
    // delayL, delayR, energyL, energyR, peakL, peakR, peakIndexL, peakIndexR).
    // Captured from this tree at commit 12d6929 on 2026-10-04 with libmysofa v1.3.2.
    // Replaces the FNV checksum 0xb5acb6188c85f737: byte-exact float hashes differ across
    // build types by one ulp (research Pitfall 6). D-17 governs any future change of these
    // values.
    constexpr HRIRFingerprint kGoldenFingerprint { 128, 11.0f, 43.0f, 5.42644277, 0.302691799, 1.26062512f, 0.268568307f, 11, 16 };
    constexpr int kGoldenIRLength = 128;
    constexpr int kGoldenNumPositions = 16020;
}

TEST_CASE ("Bernschuetz KU100 — synchronous load succeeds", "[binaural][bernschuetz][sync-load]")
{
    HRTFDatabase db;
    REQUIRE_FALSE (db.isLoaded());

    bool ok = db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate));

    REQUIRE (ok);
    REQUIRE (db.isLoaded());
    CHECK (db.getIRLength() == kGoldenIRLength);
    CHECK (db.getNumPositions() == kGoldenNumPositions);
}

TEST_CASE ("Bernschuetz KU100 — golden HRIR fingerprint at az=90deg (own control)", "[binaural][bernschuetz][golden]")
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

TEST_CASE ("Bernschuetz KU100 — negative control: unloaded database has no data", "[binaural][bernschuetz][negative-control]")
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
