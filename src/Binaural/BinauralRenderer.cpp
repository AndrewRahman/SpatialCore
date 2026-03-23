#include <SpatialCore/Binaural/BinauralRenderer.h>
#include <cstring>

namespace spatialcore
{

void BinauralRenderer::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = maxBlockSize;
}

void BinauralRenderer::setProfile(int profileIndex, HRTFDatabase& /*hrtfDb*/)
{
    activeProfile = profileIndex;
}

void BinauralRenderer::updateSourceHRIR(int /*sourceIndex*/, float /*azRad*/, float /*elRad*/,
                                         HRTFDatabase& /*db*/)
{
    // Stub
}

void BinauralRenderer::renderSourceBuffers(const float* const* /*sourceBufs*/,
                                            const bool* /*sourceEnabled*/,
                                            int /*numSources*/, int numSamples,
                                            float* outL, float* outR)
{
    std::memset(outL, 0, sizeof(float) * static_cast<size_t>(numSamples));
    std::memset(outR, 0, sizeof(float) * static_cast<size_t>(numSamples));
}

void BinauralRenderer::reset()
{
    std::memset(cachedSourceAz, 0, sizeof(cachedSourceAz));
    std::memset(cachedSourceEl, 0, sizeof(cachedSourceEl));
    std::memset(sourceConvReady, 0, sizeof(sourceConvReady));
    itdActive = false;
}

} // namespace spatialcore
