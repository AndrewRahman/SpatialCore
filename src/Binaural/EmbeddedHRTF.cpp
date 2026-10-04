#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/HRTFProfile.h>

// The generated BinaryData header. THIS is the only translation unit that includes it:
// it is generated into the build tree, consumers do not get its include path, and no
// header under include/ may name it (the generated namespace must not leak).
#include <SpatialCoreHRTFData.h>

namespace spatialcore
{

const char* HRTFDatabase::getEmbeddedProfileData (int profileIndex, int& sizeInBytes)
{
    sizeInBytes = 0;

    // Profile 0 (Simple) has no file; anything outside 1..5 is not a profile.
    if (profileIndex < 1 || profileIndex >= kNumHRTFProfileSlots)
        return nullptr;

    // getNamedResource returns nullptr for a name that was not compiled in, which is
    // how a SPATIALCORE_EMBED_ALL_HRTF=OFF build reports profiles 1..4: no #if here.
    const char* data = SpatialCoreHRTFData::getNamedResource (kHRTFProfiles[profileIndex].resourceName,
                                                               sizeInBytes);
    if (data == nullptr)
        sizeInBytes = 0;

    return data;
}

} // namespace spatialcore
