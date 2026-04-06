#include <SpatialCore/Algorithms/DBAPAlgorithm.h>

namespace spatialcore
{

//==============================================================================
// DBAPAlgorithm -- Distance-Based Amplitude Panning (Lossius et al., ICMC 2009)
//==============================================================================
void DBAPAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                   float* outputGains, int numSpeakers) const
{
    for (int s = 0; s < numSpeakers; ++s)
        outputGains[s] = 0.0f;

    if (numSpeakers == 0) return;

    constexpr float speakerRadius = 1.0f;
    float physicalDist = source.distance * speakerRadius;
    float srcX = physicalDist * std::cos (source.elevationRad) * std::sin (source.azimuthRad);
    float srcY = physicalDist * std::cos (source.elevationRad) * std::cos (source.azimuthRad);
    float srcZ = physicalDist * std::sin (source.elevationRad);

    constexpr float epsilon = 0.001f;

    float totalWeight = 0.0f;
    float weights[MAX_SPEAKERS] = {};

    for (int s = 0; s < numSpeakers; ++s)
    {
        float spkAz = ctx.layout.speakers[s].azimuthRad;
        float spkEl = ctx.layout.speakers[s].elevationRad;

        float spkX = speakerRadius * std::cos (spkEl) * std::sin (spkAz);
        float spkY = speakerRadius * std::cos (spkEl) * std::cos (spkAz);
        float spkZ = speakerRadius * std::sin (spkEl);

        float dx = srcX - spkX;
        float dy = srcY - spkY;
        float dz = srcZ - spkZ;
        float distSq = dx * dx + dy * dy + dz * dz;

        float w = 1.0f / std::max (epsilon, distSq);
        weights[s] = w;
        totalWeight += w;
    }

    if (totalWeight > 1e-12f)
    {
        float totalPower = 0.0f;
        for (int s = 0; s < numSpeakers; ++s)
        {
            float g = weights[s] / totalWeight;
            outputGains[s] = g;
            totalPower += g * g;
        }

        if (totalPower > 1e-12f)
        {
            float scale = 1.0f / std::sqrt (totalPower);
            for (int s = 0; s < numSpeakers; ++s)
                outputGains[s] *= scale;
        }
    }
}

} // namespace spatialcore
