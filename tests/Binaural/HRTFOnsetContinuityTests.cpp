#include <catch2/catch_test_macros.hpp>
#include "BinauralTestUtilities.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// Spatial-Media-Lab/OpenSpatialDelay#234 (O214): the onset used to align HRIRs must move continuously with direction.
// detectOnset's 10%-of-peak threshold jumped up to 36 samples between neighbouring SADIE
// directions (the peak flips between lobes), so crossfaded HRIRs were misaligned (rho
// 0.35..0.62) and the ITD ramp jumped by tens of samples per transition under motion.
// getAlignedHRIR now uses detectOnsetEnergy.

namespace
{
    double rho (const std::vector<float>& a, const std::vector<float>& b)
    {
        double ab = 0, aa = 0, bb = 0;
        for (size_t i = 0; i < a.size(); ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
        return ab / std::sqrt (aa * bb + 1e-30);
    }

    struct SweepStats { int maxShiftJump = 0; int jumpsOver4 = 0; double meanRho = 0.0; };

    // Sweep az 0..359 (1 degree steps) at the given elevation; neighbour = +1 degree.
    SweepStats sweep (HRTFDatabase& db, float elDeg)
    {
        const size_t n = static_cast<size_t> (db.getIRLength());
        std::vector<float> aL (n), aR (n), bL (n), bR (n);
        SweepStats s; int count = 0;
        for (int az = 0; az < 360; ++az)
        {
            float d0L, d0R, d1L, d1R;
            db.getAlignedHRIR (juce::degreesToRadians ((float) az), juce::degreesToRadians (elDeg), aL.data(), aR.data(), d0L, d0R);
            db.getAlignedHRIR (juce::degreesToRadians ((float) az + 1), juce::degreesToRadians (elDeg), bL.data(), bR.data(), d1L, d1R);
            const int jump = std::max (std::abs ((int) d1L - (int) d0L), std::abs ((int) d1R - (int) d0R));
            s.maxShiftJump = std::max (s.maxShiftJump, jump);
            s.jumpsOver4 += jump > 4;
            s.meanRho += 0.5 * (rho (aL, bL) + rho (aR, bR));
            ++count;
        }
        s.meanRho /= count;
        return s;
    }
}

TEST_CASE ("detectOnsetEnergy - basic behaviour", "[binaural][onset][234]")
{
    std::vector<float> ir (256, 0.0f);
    CHECK (HRTFDatabase::detectOnsetEnergy (ir.data(), 256) == 0.0f);          // silent
    CHECK (HRTFDatabase::detectOnsetEnergy (nullptr, 256) == 0.0f);

    // A pulse at index p gives an onset near p, and shifting the pulse shifts the onset 1:1.
    auto onsetOfPulse = [&] (int p)
    {
        std::fill (ir.begin(), ir.end(), 0.0f);
        ir[(size_t) p] = 1.0f;
        return HRTFDatabase::detectOnsetEnergy (ir.data(), 256);
    };
    const float a = onsetOfPulse (40), b = onsetOfPulse (47);
    CHECK (std::abs ((b - a) - 7.0f) < 1e-3f);
    CHECK (a >= 30.0f);
    CHECK (a <= 41.0f);

    // Continuity: a small leading precursor must not move the onset by more than a few samples
    // (a peak-relative threshold would jump to the precursor or the main lobe).
    std::fill (ir.begin(), ir.end(), 0.0f);
    ir[60] = 1.0f; ir[40] = 0.08f;
    const float withPre = HRTFDatabase::detectOnsetEnergy (ir.data(), 256);
    ir[40] = 0.12f;
    const float withPre2 = HRTFDatabase::detectOnsetEnergy (ir.data(), 256);
    CHECK (std::abs (withPre - withPre2) < 3.0f);
}

TEST_CASE ("SADIE II - aligned onset/ITD is continuous between 1 degree neighbours", "[binaural][onset][234][sadie]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile ("sadie_d2_ku100.sofa"), static_cast<float> (kTestSampleRate)));

    for (float el : { 0.0f, 30.0f, -30.0f })
    {
        const auto s = sweep (db, el);
        INFO ("el=" << el << " maxShiftJump=" << s.maxShiftJump << " meanRho=" << s.meanRho);
        // Before O214: max jump 30 samples, 354 of 1080 steps over 4, mean rho 0.68.
        CHECK (s.maxShiftJump <= 6);
        CHECK (s.jumpsOver4 == 0);
        CHECK (s.meanRho > 0.85);
    }
}

TEST_CASE ("MIT KEMAR - aligned onset/ITD stays continuous (no regression)", "[binaural][onset][234][kemar]")
{
    HRTFDatabase db;
    REQUIRE (db.loadFromFile (getSofaFile ("mit_kemar_large_pinna.sofa"), static_cast<float> (kTestSampleRate)));

    for (float el : { 0.0f, 30.0f, -30.0f })
    {
        const auto s = sweep (db, el);
        INFO ("el=" << el << " maxShiftJump=" << s.maxShiftJump << " meanRho=" << s.meanRho);
        CHECK (s.maxShiftJump <= 3);
        CHECK (s.meanRho > 0.97);
    }
}
