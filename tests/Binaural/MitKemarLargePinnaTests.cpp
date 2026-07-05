#include <catch2/catch_test_macros.hpp>
#include "BinauralTestUtilities.h"

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// MIT KEMAR Large Pinna ("Studio Reference", OSD profile 5) — own control,
// own golden. See SadieD2KU100Tests.cpp for the locked-decision rationale
// (CONTEXT.md): each profile is tested individually, never via a shared
// representative.
//
// MIT KEMAR is the ONE profile with documented special-case behavior at the
// BinauralRenderer level (do NOT "fix" these — D-09 iron rule; any suspicion
// is a GH issue, not an inline edit):
//   - ITD delay line is a pass-through: this SOFA file reports 0 raw SOFA
//     delay, so getAlignedHRIR()'s onset-detection fallback supplies the ITD
//     (CLAUDE.md Gotcha).
//   - BinauralRenderer::setProfile gates the low-shelf bass compensation on
//     profileIndex == 5 specifically (24 dB deficit at 50 Hz measurement
//     limitation) — this suite doesn't re-test BinauralRenderer::setProfile
//     directly (that's covered by BinauralRenderer's own state, not
//     HRTFDatabase), but documents the profile-index binding here since it's
//     the reason this profile gets its own dedicated golden rather than being
//     folded into a "generic SOFA profile" test.
// ============================================================================

namespace
{
    constexpr const char* kSofaFile = "mit_kemar_large_pinna.sofa";

    constexpr uint64_t kGoldenChecksum = 0xd69ffad577d8f609ULL;
    constexpr int kGoldenIRLength = 558;
    constexpr int kGoldenNumPositions = 710;
}

TEST_CASE ("MIT KEMAR Large Pinna — synchronous load succeeds", "[binaural][mitkemar][sync-load]")
{
    HRTFDatabase db;
    REQUIRE_FALSE (db.isLoaded());

    bool ok = db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate));

    REQUIRE (ok);
    REQUIRE (db.isLoaded());
    CHECK (db.getIRLength() == kGoldenIRLength);
    CHECK (db.getNumPositions() == kGoldenNumPositions);
}

TEST_CASE ("MIT KEMAR Large Pinna — golden HRIR checksum at az=90deg (own control)", "[binaural][mitkemar][golden]")
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

TEST_CASE ("MIT KEMAR Large Pinna — ITD pass-through: raw SOFA delay is zero (preserved verbatim, D-09)",
           "[binaural][mitkemar][itd-passthrough]")
{
    // CLAUDE.md Gotcha: "ITD delay line is a pass-through for MIT KEMAR. ITD
    // values are 0 in this SOFA file (embedded in HRIR waveform). Don't assume
    // ITD is active." This test locks that exact behavior in place so a future
    // transplant/refactor can't silently "fix" it without tripping this suite.
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate)));

    std::vector<float> irL (static_cast<size_t> (db.getIRLength()));
    std::vector<float> irR (static_cast<size_t> (db.getIRLength()));
    float rawDelayL = 0.0f, rawDelayR = 0.0f;

    // getInterpolatedHRIR() reports the SOFA file's raw Data.Delay values,
    // before any onset-detection fallback is applied.
    db.getInterpolatedHRIR (juce::degreesToRadians (90.0f), 0.0f, irL.data(), irR.data(), rawDelayL, rawDelayR);

    CHECK (rawDelayL < 0.001f);
    CHECK (rawDelayR < 0.001f);
}

TEST_CASE ("MIT KEMAR Large Pinna — negative control: unloaded database has no data", "[binaural][mitkemar][negative-control]")
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
