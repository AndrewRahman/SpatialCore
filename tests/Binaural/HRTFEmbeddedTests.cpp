#include <catch2/catch_test_macros.hpp>
#include "BinauralMetrics.h"

#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/HRTFProfile.h>

#include <cstdint>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// HRTF packaging tests (DATA-01).
//
// This file starts with the signature check of the source-tree SOFA files. The embedding
// tests and the resolution-chain tests (shared folder, embedded default, loud error) are
// added by Plan 03-05.
// ============================================================================

TEST_CASE ("HRTF files: every source-tree SOFA file is real HDF5, not a Git LFS pointer",
           "[hrtf-embed][signature]")
{
    // An unresolved Git LFS pointer is a ~130 byte text stub; every HRTF test after it would
    // measure nothing. The HDF5 file signature is 89 48 44 46 0D 0A 1A 0A. Mirrors the LFS
    // guard in .github/workflows/ci.yml.
    constexpr uint8_t kSignature[8] = { 0x89, 0x48, 0x44, 0x46, 0x0D, 0x0A, 0x1A, 0x0A };

    for (int profile = 1; profile <= 5; ++profile)
    {
        const juce::File file = getSofaFile (testProfileFile (profile));
        INFO (file.getFullPathName());
        REQUIRE (file.existsAsFile());
        CHECK (file.getSize() > 1000000);

        juce::FileInputStream stream (file);
        REQUIRE (stream.openedOk());

        uint8_t head[8] = {};
        REQUIRE (stream.read (head, 8) == 8);
        for (int i = 0; i < 8; ++i)
        {
            INFO ("signature byte " << i);
            CHECK (head[i] == kSignature[i]);
        }
    }
}

TEST_CASE ("HRTF embed: KEMAR's embedded bytes equal the source file and load",
           "[hrtf-embed][bytes]")
{
    int size = 0;
    const char* data = HRTFDatabase::getEmbeddedProfileData (5, size);
    REQUIRE (data != nullptr);

    const juce::File file = getSofaFile (testProfileFile (5));
    juce::MemoryBlock fileBytes;
    REQUIRE (file.loadFileAsData (fileBytes));

    CHECK (static_cast<size_t> (size) == fileBytes.getSize());
    CHECK (fnv1aHash (data, static_cast<size_t> (size)) == fnv1aHash (fileBytes.getData(), fileBytes.getSize()));

    HRTFDatabase db;
    REQUIRE (db.loadFromMemory (data, size, 48000.0f));
    CHECK (db.getIRLength() == 558);
}
