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
    // Height layouts always carry triplets because RenderEngine::activateLayout
    // aborts otherwise (D-02a); a hand-built LayoutContext with empty triplets on
    // a height layout is a caller error this reports in Debug (D-01).
    jassert (ctx.triplets.empty() == ! layoutHasHeight (ctx.layout));

    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);
}

} // namespace spatialcore
