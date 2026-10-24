#include <catch2/catch_test_macros.hpp>
#include "BinauralTestUtilities.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

    // Tolerance fingerprint of the az=90deg ITD-aligned HRIR pair (field order: irLength,
    // delayL, delayR, energyL, energyR, peakL, peakR, peakIndexL, peakIndexR).
    // Captured from this tree at commit 12d6929 on 2026-10-04 with libmysofa v1.3.2.
    // Replaces the FNV checksum 0xd69ffad577d8f609: byte-exact float hashes differ across
    // build types by one ulp (research Pitfall 6). D-17 governs any future change of these
    // values.
    // delayL/delayR are the onset SHIFT since the WR-01 fix (Spatial-Media-Lab/OpenSpatialDelay#234,
    // O212): getAlignedHRIR reports onset - minOnset, so the leading ear is 0 and the other
    // carries the ITD. Before it they were the absolute onsets 31 / 61; only these two
    // fields moved (energy, peaks and peak indices are unchanged, the aligned waveform is the same).
    constexpr HRIRFingerprint kGoldenFingerprint { 558, 0.0f, 33.0f, 2.00355748, 0.163567328, 0.515830636f, -0.121116042f, 39, 49 };
    // O214 (Spatial-Media-Lab/OpenSpatialDelay#234): onset now from detectOnsetEnergy (smooth energy onset), so the
    // lagging ear's shift and the aligned right-ear tail changed; rendered onsets are unchanged (lossless).
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

TEST_CASE ("MIT KEMAR Large Pinna — golden HRIR fingerprint at az=90deg (own control)", "[binaural][mitkemar][golden]")
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

// ============================================================================
// Regression test for Spatial-Media-Lab/OpenSpatialDelay#89 (OSD v1.0.11): MIT KEMAR stores its
// interaural time difference in the HRIR waveform and reports a zero SOFA delay for both ears.
// Without the fallback in getAlignedHRIR(), the dual-slot crossfade blended HRIRs whose ears were
// not time-aligned and comb-filtered. The fallback measures each ear's waveform onset, shifts the
// later ear back by the onset difference so the aligned pair starts together, and reports the
// onsets as the delays so the renderer's ITD delay line puts the time difference back.
//
// What the checks prove, at azimuth +90, -90 and +45 degrees at ear level:
//   - the raw pair really has zero delays and (at +-90) onsets at least 10 samples apart, so the
//     fallback has real work to do;
//   - the reported delays differ by exactly the raw onset difference (the delay is the onset);
//   - the aligned ears' onsets differ by at most 1 sample.
// Removing the zero-delay branch turns the second and third checks red (D-17).
// ============================================================================

namespace
{
    // The library's onset rule (HRTFDatabase::detectOnset with its 0.1 fraction): the first sample
    // whose magnitude is at least 0.1 x the peak. A deliberate copy, so the test states the rule
    // it expects rather than calling the code under test to check itself.
    int firstSampleAtTenPercentOfPeak (const std::vector<float>& ir)
    {
        float peak = 0.0f;
        for (float v : ir)
            peak = std::max (peak, std::abs (v));
        for (size_t i = 0; i < ir.size(); ++i)
            if (std::abs (ir[i]) >= 0.1f * peak)
                return static_cast<int> (i);
        return 0;
    }
}

TEST_CASE ("MIT KEMAR Large Pinna — Spatial-Media-Lab/OpenSpatialDelay#89: zero SOFA delays fall back to the waveform onsets, so the aligned ears start together",
           "[binaural][mitkemar][osd89]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate)));

    const size_t irLength = static_cast<size_t> (db.getIRLength());
    REQUIRE (irLength > 0);

    struct Probe { float azDeg; bool farEar; };
    const Probe directions[] = { { 90.0f, true }, { -90.0f, true }, { 45.0f, false } };

    for (const auto& dir : directions)
    {
        const float az = juce::degreesToRadians (dir.azDeg);

        std::vector<float> rawL (irLength), rawR (irLength), alignedL (irLength), alignedR (irLength);
        float rawDelayL = -1.0f, rawDelayR = -1.0f, delayL = -1.0f, delayR = -1.0f;
        db.getInterpolatedHRIR (az, 0.0f, rawL.data(), rawR.data(), rawDelayL, rawDelayR);
        db.getAlignedHRIR (az, 0.0f, alignedL.data(), alignedR.data(), delayL, delayR);

        const int rawOnsetL = firstSampleAtTenPercentOfPeak (rawL);
        const int rawOnsetR = firstSampleAtTenPercentOfPeak (rawR);
        const int alignedOnsetL = firstSampleAtTenPercentOfPeak (alignedL);
        const int alignedOnsetR = firstSampleAtTenPercentOfPeak (alignedR);

        CAPTURE (dir.azDeg, rawDelayL, rawDelayR, rawOnsetL, rawOnsetR, delayL, delayR, alignedOnsetL, alignedOnsetR);
        WARN ("Spatial-Media-Lab/OpenSpatialDelay#89 az " << dir.azDeg << ": raw delays " << rawDelayL << "/" << rawDelayR
              << ", raw onsets " << rawOnsetL << "/" << rawOnsetR << ", reported delays " << delayL << "/" << delayR
              << ", aligned onsets " << alignedOnsetL << "/" << alignedOnsetR);

        // The precondition: KEMAR's SOFA delays are zero and the ITD sits in the waveform.
        CHECK (rawDelayL < 0.001f);
        CHECK (rawDelayR < 0.001f);
        if (dir.farEar)
            CHECK (std::abs (rawOnsetR - rawOnsetL) >= 10);

        // The fallback: the reported delay of each ear is its waveform onset.
        CHECK (delayR - delayL == static_cast<float> (rawOnsetR - rawOnsetL));

        // The aligned ears start together.
        CHECK (std::abs (alignedOnsetR - alignedOnsetL) <= 1);
    }
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
