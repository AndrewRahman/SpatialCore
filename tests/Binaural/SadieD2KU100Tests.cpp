#include <catch2/catch_test_macros.hpp>
#include "BinauralTestUtilities.h"

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// SADIE II D2 KU100 ("Immersive", OSD profile 1) — own control, own golden.
// Per CONTEXT.md locked decision: each of the 6 binauralization options is
// tested INDIVIDUALLY against its own control — never a shared "representative
// profile" shortcut (each profile has custom tweaks; testing only one "sets us
// up for failure").
// ============================================================================

namespace
{
    constexpr const char* kSofaFile = "sadie_d2_ku100.sofa";

    // Golden checksum captured directly from this synchronously-loaded profile
    // via HRTFDatabase::getAlignedHRIR() at az=90deg (hard left), el=0deg — the
    // exact ITD-free HRIR path BinauralRenderer::updateSourceHRIR() uses for
    // every SOFA profile (itdActive == true). SADIE reports non-zero SOFA delays
    // natively (bypasses the onset-detection fallback other profiles use), so
    // its raw delayL/delayR values are the largest of the 5 profiles.
    constexpr uint64_t kGoldenChecksum = 0xbf6ea5b4ace12b7bULL;
    constexpr int kGoldenIRLength = 256;
    constexpr int kGoldenNumPositions = 8802;
}

TEST_CASE ("SADIE II D2 KU100 — synchronous load succeeds", "[binaural][sadie][sync-load]")
{
    HRTFDatabase db;
    REQUIRE_FALSE (db.isLoaded());

    bool ok = db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate));

    REQUIRE (ok);
    REQUIRE (db.isLoaded());
    CHECK (db.getIRLength() == kGoldenIRLength);
    CHECK (db.getNumPositions() == kGoldenNumPositions);
}

TEST_CASE ("SADIE II D2 KU100 — golden HRIR checksum at az=90deg (own control)", "[binaural][sadie][golden]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate)));

    std::vector<float> irL (static_cast<size_t> (db.getIRLength()));
    std::vector<float> irR (static_cast<size_t> (db.getIRLength()));
    float delayL = 0.0f, delayR = 0.0f;

    db.getAlignedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), delayL, delayR);

    uint64_t checksum = hashHRIRPair (irL.data(), irR.data(), db.getIRLength());
    CHECK (checksum == kGoldenChecksum);

    // SADIE reports non-zero SOFA delays natively — confirms this profile
    // bypasses the onset-detection fallback (issue Spatial-Media-Lab/OpenSpatialDelay#89), matching the
    // documented behavior in Tests/ConvolverGlitchTests.cpp.
    float maxITD = std::max (std::abs (delayL), std::abs (delayR));
    CHECK (maxITD > 0.1f);
}

TEST_CASE ("SADIE II D2 KU100 — negative control: unloaded database has no data", "[binaural][sadie][negative-control]")
{
    // Documents the synchronous-load landmine (CLAUDE.md Gotcha): a database
    // that skips loadFromFile() never becomes loaded, proving this suite
    // genuinely exercises the loaded profile rather than silently passing on
    // an empty/fallback database.
    HRTFDatabase db;
    CHECK_FALSE (db.isLoaded());
    CHECK (db.getIRLength() == 0);
    CHECK (db.getNumPositions() == 0);

    std::vector<float> irL (16, -1.0f), irR (16, -1.0f);
    float delayL = 0.0f, delayR = 0.0f;
    db.getInterpolatedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), delayL, delayR);

    // Unloaded database leaves the buffers untouched (early-return guard) —
    // proving the golden-checksum test above genuinely depends on the
    // synchronous load, not incidental zero-initialization.
    CHECK (irL[0] == -1.0f);
    CHECK (irR[0] == -1.0f);
}
