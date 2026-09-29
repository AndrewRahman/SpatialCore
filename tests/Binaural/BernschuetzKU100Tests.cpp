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

    constexpr uint64_t kGoldenChecksum = 0xb5acb6188c85f737ULL;
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

TEST_CASE ("Bernschuetz KU100 — golden HRIR checksum at az=90deg (own control)", "[binaural][bernschuetz][golden]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate)));

    std::vector<float> irL (static_cast<size_t> (db.getIRLength()));
    std::vector<float> irR (static_cast<size_t> (db.getIRLength()));
    float delayL = 0.0f, delayR = 0.0f;

    db.getAlignedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), delayL, delayR);

    uint64_t checksum = hashHRIRPair (irL.data(), irR.data(), db.getIRLength());
    CHECK (checksum == kGoldenChecksum);

    CHECK (std::abs (delayL) < std::abs (delayR));
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
