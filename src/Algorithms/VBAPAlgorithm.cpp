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
    // Height layouts always carry triplets and flat layouts never do:
    // RenderEngine::activateLayout enforces that on the message thread and
    // aborts on a mismatch (D-02a). There is deliberately no assert here
    // (IN-14): this runs on the audio thread, and JUCE's assertion path logs
    // and allocates. A hand-built LayoutContext that breaks the invariant
    // still gets a defined, finite result: an empty list pans by 2D VBAP
    // (azimuth only), a non-empty list by 3D VBAP.

    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);
}

} // namespace spatialcore
