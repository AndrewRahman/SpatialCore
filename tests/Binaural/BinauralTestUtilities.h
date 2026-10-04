#pragma once

// Shared helpers for the per-profile binaural test suite (Phase 8 Plan 08-04).
//
// LANDMINE (CLAUDE.md Gotcha, replicated from Tests/ConvolverGlitchTests.cpp:334-379
// `createBinauralProcessor()` / `testLoadHRTFProfile()`): every SOFA-profile test in
// this directory MUST call HRTFDatabase::loadFromFile() synchronously before asserting
// isLoaded()/checksums. There is no timer thread to race here (these tests call
// HRTFDatabase/BinauralRenderer directly, not through an AudioProcessor), but the
// synchronous-load discipline is still the point being proven: Test 3 in each
// SOFA-profile file is a negative control showing what an unloaded database looks
// like, so the suite can never silently regress into asserting against a database
// that was never actually populated.

#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace spatialcore::test
{

// FNV-1a 64-bit hash over raw bytes — same dependency-free algorithm used by
// Tests/PreCleanVerify.cpp's golden-checksum harness (no JUCE cryptography module
// linked into SpatialCoreTests, so this avoids adding a new dependency for a
// verification-only checksum).
inline uint64_t fnv1aHash (const void* data, size_t numBytes, uint64_t seed = 1469598103934665603ULL)
{
    const auto* bytes = static_cast<const uint8_t*> (data);
    uint64_t hash = seed;
    constexpr uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < numBytes; ++i)
    {
        hash ^= bytes[i];
        hash *= prime;
    }
    return hash;
}

inline uint64_t hashHRIRPair (const float* irL, const float* irR, int irLength)
{
    uint64_t hash = fnv1aHash (irL, static_cast<size_t> (irLength) * sizeof (float));
    hash = fnv1aHash (irR, static_cast<size_t> (irLength) * sizeof (float), hash);
    return hash;
}

// ---------------------------------------------------------------------------
// Tolerance fingerprint of an aligned HRIR pair (Phase 3 Plan 03-02).
//
// Replaces the byte-exact FNV hash as the per-profile golden comparison: a hash of raw
// float bytes differs between Debug and Release by about 1 ulp on a single sample
// (research Pitfall 6), and it can only say "different", never "by how much". The
// fingerprint keeps the quantities that matter for the rendered sound — IR length, the
// two aligned-out delays, per-ear energy, per-ear peak value and its index — and
// compares them with tolerances far below anything audible. Tolerances are about 100x
// the measured build-type difference; do NOT widen them to make a failure go away.
// ---------------------------------------------------------------------------
struct HRIRFingerprint
{
    int    irLength;
    float  delayL, delayR;      // samples, fractional
    double energyL, energyR;    // sum of squares, accumulated in double
    float  peakL, peakR;        // largest absolute sample, stored signed
    int    peakIndexL, peakIndexR;
};

namespace detail
{
    inline void peakOf (const float* ir, int irLength, float& peak, int& peakIndex, double& energy)
    {
        peak = 0.0f;
        peakIndex = 0;
        energy = 0.0;
        for (int i = 0; i < irLength; ++i)
        {
            energy += static_cast<double> (ir[i]) * static_cast<double> (ir[i]);
            if (std::abs (ir[i]) > std::abs (peak))
            {
                peak = ir[i];
                peakIndex = i;
            }
        }
    }
}

inline HRIRFingerprint fingerprintHRIRPair (const float* irL, const float* irR, int irLength,
                                            float delayL, float delayR)
{
    HRIRFingerprint fp {};
    fp.irLength = irLength;
    fp.delayL = delayL;
    fp.delayR = delayR;
    detail::peakOf (irL, irLength, fp.peakL, fp.peakIndexL, fp.energyL);
    detail::peakOf (irR, irLength, fp.peakR, fp.peakIndexR, fp.energyR);
    return fp;
}

inline HRIRFingerprint fingerprintAlignedHRIR (const HRTFDatabase& db, float azDeg, float elDeg)
{
    std::vector<float> irL (static_cast<size_t> (db.getIRLength()));
    std::vector<float> irR (static_cast<size_t> (db.getIRLength()));
    float delayL = 0.0f, delayR = 0.0f;
    db.getAlignedHRIR (juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg),
                       irL.data(), irR.data(), delayL, delayR);
    return fingerprintHRIRPair (irL.data(), irR.data(), db.getIRLength(), delayL, delayR);
}

// Tolerances: lengths and peak indices exact, delays 1e-4 samples, energies and peak
// values 1e-5 relative. On mismatch, whyNot (if given) names the first failing field
// with both values.
inline bool fingerprintMatches (const HRIRFingerprint& actual, const HRIRFingerprint& expected,
                                std::string* whyNot = nullptr)
{
    constexpr double kDelayTol = 1.0e-4;
    constexpr double kRelTol   = 1.0e-5;

    auto fail = [&] (const char* field, double a, double e)
    {
        if (whyNot != nullptr)
        {
            char buf[160];
            std::snprintf (buf, sizeof (buf), "%s: actual %.9g, expected %.9g", field, a, e);
            *whyNot = buf;
        }
        return false;
    };
    auto relOk = [] (double a, double e)
    {
        return std::abs (a - e) <= kRelTol * std::max (std::abs (e), 1.0e-30);
    };

    if (actual.irLength != expected.irLength)           return fail ("irLength", actual.irLength, expected.irLength);
    if (actual.peakIndexL != expected.peakIndexL)       return fail ("peakIndexL", actual.peakIndexL, expected.peakIndexL);
    if (actual.peakIndexR != expected.peakIndexR)       return fail ("peakIndexR", actual.peakIndexR, expected.peakIndexR);
    if (std::abs (static_cast<double> (actual.delayL) - expected.delayL) > kDelayTol)
        return fail ("delayL", actual.delayL, expected.delayL);
    if (std::abs (static_cast<double> (actual.delayR) - expected.delayR) > kDelayTol)
        return fail ("delayR", actual.delayR, expected.delayR);
    if (! relOk (actual.energyL, expected.energyL))     return fail ("energyL", actual.energyL, expected.energyL);
    if (! relOk (actual.energyR, expected.energyR))     return fail ("energyR", actual.energyR, expected.energyR);
    if (! relOk (actual.peakL, expected.peakL))         return fail ("peakL", actual.peakL, expected.peakL);
    if (! relOk (actual.peakR, expected.peakR))         return fail ("peakR", actual.peakR, expected.peakR);

    if (whyNot != nullptr)
        whyNot->clear();
    return true;
}

// One-line, full-precision rendering of every field — used to capture golden constants.
inline std::string printFingerprint (const HRIRFingerprint& fp)
{
    char buf[400];
    std::snprintf (buf, sizeof (buf),
                   "{ %d, %.9gf, %.9gf, %.9g, %.9g, %.9gf, %.9gf, %d, %d }",
                   fp.irLength, fp.delayL, fp.delayR, fp.energyL, fp.energyR,
                   fp.peakL, fp.peakR, fp.peakIndexL, fp.peakIndexR);
    return buf;
}

// Resolves a profile's LFS-tracked raw .sofa file under SpatialCore/HRTF/.
// SPATIALCORE_HRTF_DIR is injected by tests/CMakeLists.txt as an absolute path to
// the checked-out HRTF/ directory (Git-LFS content, not BinaryData).
inline juce::File getSofaFile (const char* filename)
{
    return juce::File (SPATIALCORE_HRTF_DIR).getChildFile (filename);
}

static constexpr double kTestSampleRate = 48000.0;

} // namespace spatialcore::test
