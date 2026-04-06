#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Core/SpatialMath.h>

namespace spatialcore
{

//==============================================================================
// VBIPAlgorithm -- intensity-weighted VBAP (squared gains for tighter focus)
//==============================================================================
void VBIPAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                   float* outputGains, int numSpeakers) const
{
    // Start with VBAP gains
    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
    else if (layoutHasHeight (ctx.layout))
    {
        // v1.0: Guard -- never use 2D fallback on 3D layouts
        jassertfalse;
        nearestSpeaker3DFallback (ctx.layout, source.azimuthRad, source.elevationRad,
                                  outputGains, numSpeakers);
    }
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);

    // Square all gains for intensity weighting
    float sum = 0.0f;
    for (int s = 0; s < numSpeakers; ++s)
    {
        outputGains[s] = outputGains[s] * outputGains[s];
        sum += outputGains[s];
    }

    // Normalize to constant power
    if (sum > 1e-12f)
    {
        float scale = 1.0f / std::sqrt (sum);
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] *= scale;
    }
}

} // namespace spatialcore
