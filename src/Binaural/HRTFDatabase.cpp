#include <SpatialCore/Binaural/HRTFDatabase.h>

namespace spatialcore
{

HRTFDatabase::HRTFDatabase() = default;

HRTFDatabase::~HRTFDatabase()
{
    unload();
}

bool HRTFDatabase::loadFromMemory(const void* /*data*/, int /*dataSize*/, float /*targetSampleRate*/)
{
    // Stub — real SOFA loading via libmysofa during extraction
    return false;
}

void HRTFDatabase::getInterpolatedHRIR(float /*azimuthRad*/, float /*elevationRad*/,
                                        float* /*irL*/, float* /*irR*/,
                                        float& delayL, float& delayR) const
{
    delayL = 0.0f;
    delayR = 0.0f;
}

void HRTFDatabase::getAlignedHRIR(float /*azimuthRad*/, float /*elevationRad*/,
                                   float* /*irL*/, float* /*irR*/,
                                   float& delayL, float& delayR) const
{
    delayL = 0.0f;
    delayR = 0.0f;
}

void HRTFDatabase::unload()
{
    loaded = false;
    irLength = 0;
    numPositions = 0;
    easyHandle = nullptr;
}

} // namespace spatialcore
