#include <SpatialCore/IO/AmbisonicsCodec.h>
#include <cmath>
#include <cstring>

namespace spatialcore
{

void AmbisonicsCodec::encode(const SourcePosition& /*source*/, int /*order*/,
                             float* shCoeffs, int numCoeffs)
{
    std::memset(shCoeffs, 0, sizeof(float) * static_cast<size_t>(numCoeffs));
}

void AmbisonicsCodec::getDecodeMatrix(int /*order*/, int /*numSpeakers*/,
                                      const float* /*speakerAzimuths*/,
                                      const float* /*speakerElevations*/,
                                      float* /*decodeMatrix*/)
{
    // Stub — real decode matrix computation during extraction
}

float AmbisonicsCodec::evaluateSH(int /*l*/, int /*m*/,
                                  float /*azimuthRad*/, float /*elevationRad*/)
{
    return 0.0f;
}

void AmbisonicsCodec::applyMaxREWeights(float* /*shCoeffs*/, int /*order*/)
{
    // Stub — real max-rE weighting during extraction
}

} // namespace spatialcore
