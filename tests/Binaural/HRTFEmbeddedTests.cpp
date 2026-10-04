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

namespace
{
    /** IR length of each built-in profile at 48 kHz, index = profile (0 unused). */
    constexpr int kExpectedIRLength[6] = { 0, 256, 218, 279, 128, 558 };

    /** True when profile p is compiled into this build. */
    constexpr bool profileIsEmbedded (int p)
    {
#if SPATIALCORE_EMBEDS_ALL_HRTF
        return p >= 1 && p <= 5;
#else
        return p == kAlwaysEmbeddedHRTFProfile;
#endif
    }
}

TEST_CASE ("HRTF embed: embedded bytes equal the source file and load",
           "[hrtf-embed][bytes]")
{
    for (int profile = 1; profile <= 5; ++profile)
    {
        INFO ("profile " << profile);
        int size = 0;
        const char* data = HRTFDatabase::getEmbeddedProfileData (profile, size);

        if (! profileIsEmbedded (profile))
        {
            // OFF build: profiles 1-4 are simply absent, reported without any #if in the lookup.
            CHECK (data == nullptr);
            CHECK (size == 0);
            continue;
        }

        REQUIRE (data != nullptr);

        const juce::File file = getSofaFile (testProfileFile (profile));
        juce::MemoryBlock fileBytes;
        REQUIRE (file.loadFileAsData (fileBytes));

        CHECK (static_cast<size_t> (size) == fileBytes.getSize());
        CHECK (fnv1aHash (data, static_cast<size_t> (size)) == fnv1aHash (fileBytes.getData(), fileBytes.getSize()));

        HRTFDatabase db;
        REQUIRE (db.loadFromMemory (data, size, 48000.0f));
        CHECK (db.getIRLength() == kExpectedIRLength[profile]);
    }
}

TEST_CASE ("HRTF embed: the profile table matches the files and the D-07 numbering",
           "[hrtf-embed][table]")
{
    CHECK (kNumHRTFProfileSlots == 6);
    CHECK (kHRTFProfiles[0].fileName == nullptr);
    CHECK (juce::String (kHRTFProfiles[0].displayName) == "Simple (Low CPU)");

    // D-07: display names and numbering are persisted by consumers and never change.
    const char* const expectedNames[6] = { "Simple (Low CPU)", "Immersive", "Natural", "Precise", "Spatial", "Studio Reference" };
    for (int p = 0; p < kNumHRTFProfileSlots; ++p)
        CHECK (juce::String (kHRTFProfiles[p].displayName) == expectedNames[p]);

    for (int p = 1; p <= 5; ++p)
    {
        INFO ("profile " << p);
        const juce::String fileName (kHRTFProfiles[p].fileName);
        CHECK (fileName == juce::String (testProfileFile (p)));
        CHECK (juce::String (kHRTFProfiles[p].resourceName) == fileName.replaceCharacter ('.', '_'));
        CHECK (getSofaFile (kHRTFProfiles[p].fileName).existsAsFile());
    }

    CHECK (isValidHRTFProfileIndex (0));
    CHECK (isValidHRTFProfileIndex (5));
    CHECK_FALSE (isValidHRTFProfileIndex (-1));
    CHECK_FALSE (isValidHRTFProfileIndex (6));
}

TEST_CASE ("HRTF embed: loadFromBinaryData loads each embedded profile by number",
           "[hrtf-embed][load]")
{
    HRTFDatabase db;

    for (int profile = 1; profile <= 5; ++profile)
    {
        INFO ("profile " << profile);
        const bool ok = db.loadFromBinaryData (profile, 48000.0f);
        CHECK (ok == profileIsEmbedded (profile));

        if (profileIsEmbedded (profile))
        {
            CHECK (db.isLoaded());
            CHECK (db.getIRLength() == kExpectedIRLength[profile]);
        }
        else
        {
            CHECK_FALSE (db.isLoaded());
        }
    }

    // Anything that is not a file-backed profile fails and leaves the database unloaded,
    // even when it held a profile a moment ago.
    for (const int bad : { 0, -1, 6, 100 })
    {
        INFO ("index " << bad);
        REQUIRE (db.loadFromBinaryData (kAlwaysEmbeddedHRTFProfile, 48000.0f));
        CHECK_FALSE (db.loadFromBinaryData (bad, 48000.0f));
        CHECK_FALSE (db.isLoaded());
    }
}
