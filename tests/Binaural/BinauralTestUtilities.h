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
#include <cstdint>
#include <cstddef>

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

// Resolves a profile's LFS-tracked raw .sofa file under SpatialCore/HRTF/.
// SPATIALCORE_HRTF_DIR is injected by tests/CMakeLists.txt as an absolute path to
// the checked-out HRTF/ directory (Git-LFS content, not BinaryData).
inline juce::File getSofaFile (const char* filename)
{
    return juce::File (SPATIALCORE_HRTF_DIR).getChildFile (filename);
}

static constexpr double kTestSampleRate = 48000.0;

} // namespace spatialcore::test
