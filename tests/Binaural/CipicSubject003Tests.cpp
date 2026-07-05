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

    // Golden checksum captured via HRTFDatabase::getAlignedHRIR() at az=90deg
    // (hard left), el=0deg — same path/azimuth as every other profile in this
    // suite, so cross-profile comparisons of the printed delay values remain
    // meaningful even though checksums are never compared across profiles.
    constexpr uint64_t kGoldenChecksum = 0xec93148ea0ba8fe8ULL;
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

TEST_CASE ("CIPIC Subject 003 — golden HRIR checksum at az=90deg (own control)", "[binaural][cipic][golden]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate)));

    std::vector<float> irL (static_cast<size_t> (db.getIRLength()));
    std::vector<float> irR (static_cast<size_t> (db.getIRLength()));
    float delayL = 0.0f, delayR = 0.0f;

    db.getAlignedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), delayL, delayR);

    uint64_t checksum = hashHRIRPair (irL.data(), irR.data(), db.getIRLength());
    CHECK (checksum == kGoldenChecksum);

    // Hard-left source: left ear (near side) should report a smaller ITD
    // magnitude than the right ear (far side) for a physically plausible head model.
    CHECK (std::abs (delayL) < std::abs (delayR));
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
