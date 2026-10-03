#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
#include <SpatialCore/IO/SpeakerLayout.h>

#include <algorithm>
#include <cmath>

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

    if (numSpeakers <= 0) return;

    // WR-08: non-finite input follows explicit rules, not std::max argument
    // order (realtime-safe: stack values only, no allocation or logging).
    //  - Non-finite azimuth or elevation: there is no direction to pan to, so
    //    every speaker gets the same gain, 1/sqrt(N) (unit power).
    //  - Non-finite distance (NaN, +Inf, -Inf): treated as 0.5, the
    //    SourcePosition default and the value RenderEngine renders for a
    //    distance that has never been finite, so the direction pans normally.
    //  - Finite distance: clamped to [-kMaxDistance, kMaxDistance] so the d^2
    //    arithmetic below cannot overflow to Inf (which would zero every
    //    weight). Normalised distances are 0..1, so this never moves one.
    if (! std::isfinite (source.azimuthRad) || ! std::isfinite (source.elevationRad))
    {
        const float equalGain = 1.0f / std::sqrt (static_cast<float> (numSpeakers));
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] = equalGain;
        return;
    }

    constexpr float kNonFiniteDistance = 0.5f;
    constexpr float kMaxDistance       = 1000.0f;
    const float distance = std::isfinite (source.distance)
                             ? std::clamp (source.distance, -kMaxDistance, kMaxDistance)
                             : kNonFiniteDistance;

    constexpr float speakerRadius = 1.0f;
    float physicalDist = distance * speakerRadius;
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
