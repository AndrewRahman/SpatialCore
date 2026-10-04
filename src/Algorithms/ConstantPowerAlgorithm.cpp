#include <SpatialCore/Algorithms/ConstantPowerAlgorithm.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
#include <SpatialCore/IO/SpeakerLayout.h>
#include <algorithm>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// ConstantPowerAlgorithm -- Cosine-distance all-speaker panning
//==============================================================================
void ConstantPowerAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                            float* outputGains, int numSpeakers) const
{
    for (int s = 0; s < numSpeakers; ++s)
        outputGains[s] = 0.0f;

    if (numSpeakers == 0) return;

    // Non-finite direction -> silence (D-06, IN-01). Explicit, so it no longer
    // rests on jlimit passing NaN through and std::max (0.0f, NaN) returning
    // its first argument.
    if (! std::isfinite (source.azimuthRad) || ! std::isfinite (source.elevationRad))
        return;

    float px = std::cos (source.elevationRad) * std::sin (source.azimuthRad);
    float py = std::cos (source.elevationRad) * std::cos (source.azimuthRad);
    float pz = std::sin (source.elevationRad);

    float totalPower = 0.0f;

    for (int s = 0; s < numSpeakers; ++s)
    {
        float sx = std::cos (ctx.layout.speakers[s].elevationRad) * std::sin (ctx.layout.speakers[s].azimuthRad);
        float sy = std::cos (ctx.layout.speakers[s].elevationRad) * std::cos (ctx.layout.speakers[s].azimuthRad);
        float sz = std::sin (ctx.layout.speakers[s].elevationRad);

        float dot = juce::jlimit (-1.0f, 1.0f, px * sx + py * sy + pz * sz);
        float rawGain = std::max (0.0f, dot);
        outputGains[s] = rawGain;
        totalPower += rawGain * rawGain;
    }

    if (totalPower > 1e-12f)
    {
        float scale = 1.0f / std::sqrt (totalPower);
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] *= scale;
    }
}

} // namespace spatialcore
