#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Core/SpatialMath.h>

namespace spatialcore
{

//==============================================================================
// VBAPAlgorithm -- Vector Base Amplitude Panning (2D or 3D)
//==============================================================================
void VBAPAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                   float* outputGains, int /*numSpeakers*/) const
{
    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
    else if (layoutHasHeight (ctx.layout))
    {
        // v1.0: Guard -- never use 2D fallback on 3D layouts (would route to height speakers)
        jassertfalse;  // Triplets should be populated for height layouts -- investigate
        nearestSpeaker3DFallback (ctx.layout, source.azimuthRad, source.elevationRad,
                                  outputGains, ctx.layout.numSpeakers);
    }
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);
}

} // namespace spatialcore
