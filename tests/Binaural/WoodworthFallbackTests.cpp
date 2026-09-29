#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <SpatialCore/Core/Types.h>
#include <array>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Woodworth ITD/ILD fallback (the 6th binauralization option, OSD profile 0
// "Simple (Low CPU)") — own control, no SOFA file involved. Per CONTEXT.md
// locked decision: each of the 6 binauralization options is tested
// INDIVIDUALLY against its own control; this is the CPU-lite path's control.
//
// Uses the real 5-entry binauralProfiles head-model table (verbatim values
// from Source/PluginProcessor.cpp's OpenSpatialDelayProcessor::binauralProfiles,
// moved to <SpatialCore/Core/Types.h> in Plan 08-02) rather than a synthetic
// test profile, so this suite matches the production Simple-mode path exactly.
// ============================================================================

namespace
{
    // Verbatim from Source/PluginProcessor.cpp (OpenSpatialDelayProcessor::binauralProfiles).
    // profileIndex 1-5 in BinauralContext maps to array index (profileIndex - 1).
    const std::array<BinauralProfile, 5> kBinauralProfiles = {{
        { 0.0920f, 1.3f, 1200.0f, "Immersive" },
        { 0.0850f, 0.8f, 1800.0f, "Natural" },
        { 0.0900f, 1.1f, 1400.0f, "Precise" },
        { 0.0875f, 1.5f, 1100.0f, "Spatial" },
        { 0.0875f, 1.0f, 1500.0f, "Studio Reference" },
    }};

    SourcePosition makeSource (float azDeg, float elDeg, float distance)
    {
        return { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), distance };
    }
}

TEST_CASE ("Woodworth fallback — center source (az=0) produces equal, undelayed L/R gains",
           "[binaural][woodworth][golden]")
{
    DirectBinauralAlgorithm algo;
    // profileIndex=1 -> kBinauralProfiles[0] ("Immersive")
    BinauralContext ctx { 1, 48000.0, kBinauralProfiles.data() };

    auto gains = algo.computeBinauralGains (makeSource (0.0f, 0.0f, 0.5f), ctx);

    CHECK_THAT (gains.leftGain, WithinAbs (gains.rightGain, 1e-6f));
    CHECK_THAT (gains.leftDelaySamples, WithinAbs (0.0f, 1e-6f));
    CHECK_THAT (gains.rightDelaySamples, WithinAbs (0.0f, 1e-6f));
}

TEST_CASE ("Woodworth fallback — hard-left source (az=90) delays and attenuates the far ear",
           "[binaural][woodworth][golden]")
{
    DirectBinauralAlgorithm algo;
    BinauralContext ctx { 1, 48000.0, kBinauralProfiles.data() };

    auto gains = algo.computeBinauralGains (makeSource (90.0f, 0.0f, 0.5f), ctx);

    // Source on the LEFT: left ear (near) is undelayed and unattenuated
    // relative to the right ear (far), which gets both ITD and ILD applied.
    CHECK (gains.rightGain < gains.leftGain);
    CHECK (gains.rightDelaySamples > 0.0f);
    CHECK_THAT (gains.leftDelaySamples, WithinAbs (0.0f, 1e-6f));

    // Golden ITD value for kBinauralProfiles[0] ("Immersive", headRadius=0.0920m)
    // at az=90: lateral = sin(90deg) = 1.0, itdSeconds = (0.0920/343) *
    // (asin(1)+1) = (0.0920/343) * (pi/2 + 1) ~= 6.8973e-4s -> samples @48kHz.
    float lateral = 1.0f;
    float itdSeconds = (kBinauralProfiles[0].headRadius / 343.0f)
                      * (std::abs (lateral) + std::asin (std::abs (lateral)));
    float expectedITDSamples = itdSeconds * 48000.0f;
    CHECK_THAT (gains.rightDelaySamples, WithinAbs (expectedITDSamples, 1e-3f));
}

TEST_CASE ("Woodworth fallback — hard-right source (az=-90) mirrors the hard-left case",
           "[binaural][woodworth][golden]")
{
    DirectBinauralAlgorithm algo;
    BinauralContext ctx { 1, 48000.0, kBinauralProfiles.data() };

    auto gainsLeft  = algo.computeBinauralGains (makeSource (90.0f, 0.0f, 0.5f), ctx);
    auto gainsRight = algo.computeBinauralGains (makeSource (-90.0f, 0.0f, 0.5f), ctx);

    // Mirror symmetry: L/R swapped between hard-left and hard-right.
    CHECK_THAT (gainsLeft.leftGain, WithinAbs (gainsRight.rightGain, 1e-6f));
    CHECK_THAT (gainsLeft.rightGain, WithinAbs (gainsRight.leftGain, 1e-6f));
    CHECK_THAT (gainsLeft.leftDelaySamples, WithinAbs (gainsRight.rightDelaySamples, 1e-6f));
    CHECK_THAT (gainsLeft.rightDelaySamples, WithinAbs (gainsRight.leftDelaySamples, 1e-6f));
}

TEST_CASE ("Woodworth fallback — no SOFA file involved (CPU-lite, negative control)",
           "[binaural][woodworth][negative-control]")
{
    // Documents that the Woodworth path never touches HRTFDatabase/libmysofa —
    // it is a closed-form model requiring no synchronous file load at all.
    // This is the structural negative control proving option 6 is genuinely
    // distinct from the 5 SOFA-file options tested elsewhere in this directory.
    DirectBinauralAlgorithm algo;
    BinauralContext ctx { 1, 48000.0, kBinauralProfiles.data() };

    // Calling computeBinauralGains with no prior file I/O still produces a
    // fully-defined result — proving the CPU-lite path has no load-order
    // dependency (unlike the 5 SOFA profiles, which require loadFromFile()
    // before any HRIR data is available).
    auto gains = algo.computeBinauralGains (makeSource (45.0f, 0.0f, 0.5f), ctx);
    CHECK (std::isfinite (gains.leftGain));
    CHECK (std::isfinite (gains.rightGain));
    CHECK (std::isfinite (gains.leftDelaySamples));
    CHECK (std::isfinite (gains.rightDelaySamples));
}
