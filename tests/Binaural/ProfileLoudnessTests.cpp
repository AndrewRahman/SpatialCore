#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "BinauralMetrics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Perceived level of each built-in profile (D-14: measure now, change later).
//
// This file records how loud each profile is, K-weighted per ITU-R BS.1770, asserts
// sanity (every value finite, every direction louder than -70 LKFS) and the BS.1770
// known answer for the meter itself, and pins each of profiles 1-5 to its measured mean
// loudness within 0.5 LU, with the spread among them at most 2.98 LU.
//
// D-14: tolerance approved by the user on 2026-10-04 from the Phase 3 measurement (D-14).
// No built-in level is changed in Phase 3; the bounds only fail a later change that makes
// a profile louder, quieter or further from the others than measured today. Simple
// (profile 0) is excluded: it folds its own distance gain in, so it is not comparable.
// ============================================================================

namespace
{
    constexpr double kRate = 48000.0;
    constexpr const char* kProfileNames[] = { "Simple", "SADIE", "CIPIC", "HUTUBS", "Bernschuetz", "KEMAR" };

    // Measured mean LKFS per profile on 2026-10-04 (Phase 3, D-14). Index 0 (Simple) is not pinned.
    constexpr double kMeasuredMeanLKFS[6] = { 0.0, -20.83, -19.80, -20.16, -19.34, -18.34 };
    constexpr double kLoudnessToleranceLU = 0.5;
    constexpr double kMaxSpreadLU = 2.98;   // measured 2.48 plus 0.5

    // 12 directions: 8 azimuths at ear level, two above, two below.
    constexpr Direction kDirections[12] = {
        { 0.0f, 0.0f }, { 45.0f, 0.0f }, { 90.0f, 0.0f }, { 135.0f, 0.0f },
        { 180.0f, 0.0f }, { 225.0f, 0.0f }, { 270.0f, 0.0f }, { 315.0f, 0.0f },
        { 0.0f, 45.0f }, { 180.0f, 45.0f }, { 90.0f, -30.0f }, { 270.0f, -30.0f }
    };

    struct ProfileLoudness
    {
        double overall = 0.0;
        double lowest = 0.0;
        double highest = 0.0;
        bool allFinite = true;
        bool aboveFloor = true;
    };

    ProfileLoudness measureProfile (int profile)
    {
        RenderEngine engine;
        engine.prepare (kRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        REQUIRE (loadProfileIntoActiveRenderer (engine, profile, kRate));

        const BinauralPath path = (profile == 0) ? BinauralPath::Simple : BinauralPath::HRTF;
        const RenderBlockContext ctx = makeBinauralContext (path, kRate);

        constexpr size_t kSettle = 12000;     // 0.25 s
        constexpr size_t kMeasured = 24000;   // 0.5 s

        ProfileLoudness result;
        result.lowest = std::numeric_limits<double>::infinity();
        result.highest = -std::numeric_limits<double>::infinity();

        StereoSignal concatenated;
        for (const Direction& dir : kDirections)
        {
            const std::vector<float> noise = pinkNoise (kSettle + kMeasured, 3, 0.25f);
            const StereoSignal rendered = renderThroughEngine (engine, ctx, noise, { 512 },
                                                                [dir] (int64_t) { return dir; });

            StereoSignal segment;
            segment.left.assign (rendered.left.begin() + static_cast<std::ptrdiff_t> (kSettle), rendered.left.end());
            segment.right.assign (rendered.right.begin() + static_cast<std::ptrdiff_t> (kSettle), rendered.right.end());

            const double lkfs = kWeightedLoudnessLKFS (segment, kRate);
            result.allFinite = result.allFinite && std::isfinite (lkfs);
            result.aboveFloor = result.aboveFloor && (lkfs > -70.0);
            result.lowest = std::min (result.lowest, lkfs);
            result.highest = std::max (result.highest, lkfs);

            concatenated.left.insert (concatenated.left.end(), segment.left.begin(), segment.left.end());
            concatenated.right.insert (concatenated.right.end(), segment.right.begin(), segment.right.end());
        }

        result.overall = kWeightedLoudnessLKFS (concatenated, kRate);
        result.allFinite = result.allFinite && std::isfinite (result.overall);
        return result;
    }
}

TEST_CASE ("Loudness: K-weighting reads the BS.1770 known answer", "[loudness][k-weighting]")
{
    // BS.1770: a full-scale 997 Hz sine in one channel only reads -3.01 LKFS.
    StereoSignal s;
    s.left = sineWave (3 * 48000, 997.0, kRate, 1.0f);
    s.right.assign (s.left.size(), 0.0f);

    const double lkfs = kWeightedLoudnessLKFS (s, kRate);
    INFO ("measured " << lkfs << " LKFS");
    CHECK_THAT (lkfs, WithinAbs (-3.01, 0.05));
}

TEST_CASE ("Loudness: perceived level of each built-in profile, recorded for the future standard",
           "[loudness]")
{
    ProfileLoudness results[6];

    for (int profile = 0; profile <= 5; ++profile)
    {
        results[profile] = measureProfile (profile);

        // Kept under 80 columns so Catch2 does not wrap the table.
        char line[200];
        std::snprintf (line, sizeof (line), "loudness %d %-11s %7.2f LKFS (min %7.2f, max %7.2f)",
                       profile, kProfileNames[profile], results[profile].overall,
                       results[profile].lowest, results[profile].highest);
        WARN (line);

        INFO ("profile " << profile << " " << kProfileNames[profile]);
        CHECK (results[profile].allFinite);
        CHECK (results[profile].aboveFloor);

        // D-14: tolerance approved by the user on 2026-10-04 from the Phase 3 measurement (D-14).
        if (profile >= 1)
            CHECK_THAT (results[profile].overall,
                        WithinAbs (kMeasuredMeanLKFS[profile], kLoudnessToleranceLU));
    }

    // Spread among the five built-ins. Simple is printed above but excluded here: it folds
    // its own distance gain in, so it is not comparable.
    double lowest = std::numeric_limits<double>::infinity();
    double highest = -std::numeric_limits<double>::infinity();
    for (int profile = 1; profile <= 5; ++profile)
    {
        lowest = std::min (lowest, results[profile].overall);
        highest = std::max (highest, results[profile].overall);
    }

    const double spread = highest - lowest;
    char spreadLine[120];
    std::snprintf (spreadLine, sizeof (spreadLine), "loudness spread among profiles 1-5: %.2f LU (D-14)", spread);
    WARN (spreadLine);

    CHECK (std::isfinite (spread));
    CHECK (spread <= kMaxSpreadLU);
}
