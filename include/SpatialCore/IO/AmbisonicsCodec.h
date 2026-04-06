#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>

namespace spatialcore
{

class AmbisonicsCodec
{
public:
    static constexpr int MAX_AMBI_ORDER = 6;
    static constexpr int MAX_AMBI_CHANNELS = (MAX_AMBI_ORDER + 1) * (MAX_AMBI_ORDER + 1);

    static void encode(const SourcePosition& source, int order,
                       float* shCoeffs, int numCoeffs);

    static void getDecodeMatrix(int order, int numSpeakers,
                                const float* speakerAzimuths,
                                const float* speakerElevations,
                                float* decodeMatrix);

    static float evaluateSH(int l, int m, float azimuthRad, float elevationRad);

    static void applyMaxREWeights(float* shCoeffs, int order);
};

} // namespace spatialcore
