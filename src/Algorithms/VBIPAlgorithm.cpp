#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Core/SpatialMath.h>
#include <algorithm>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// VBIPAlgorithm -- Vector Base Intensity Panning (Pernaux, Boussard & Jot,
// DAFx-98 sec. 2.2.2). VBAP gains raised to exponent 1/2, then renormalised so
// the sum of squares is 1: g_i = sqrt (G_i / sum G_j), G = L^-1 p. This aims the
// energy vector rE at the source (VBAP aims the velocity vector rV) and is wider
// than VBAP between speakers.
//
// Single band: the paper pairs VBAP below 700 Hz with VBIP above 700 Hz;
// SpatialCore applies the VBIP half at all frequencies (D-15, tracked in
// AndrewRahman/SpatialCore#20). Below the horizon on height layouts the
// transform acts on the EAR-downmixed VBAP vector (D-04).
//==============================================================================
void VBIPAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                   float* outputGains, int numSpeakers) const
{
    // Height layouts always carry triplets and flat layouts never do:
    // RenderEngine::activateLayout enforces that on the message thread and
    // aborts on a mismatch (D-02a). There is deliberately no assert here
    // (IN-14): this runs on the audio thread, and JUCE's assertion path logs
    // and allocates. A hand-built LayoutContext that breaks the invariant
    // still gets a defined, finite result: an empty list pans by 2D VBAP
    // (azimuth only), a non-empty list by 3D VBAP.

    // Start with VBAP gains (proportional to G = L^-1 p over the active pair/triplet)
    if (! ctx.triplets.empty())
        computeVBAPGains3D (ctx.layout, ctx.triplets, source.azimuthRad, source.elevationRad, outputGains);
    else
        computeVBAPGains2D (ctx.layout, source.azimuthRad, outputGains);

    // Exponent 1/2 (D-14): g_i -> sqrt (g_i). Because the VBAP gains are a positive
    // multiple of G, renormalising the square roots to unit power gives exactly
    // sqrt (G_i / sum G_j).
    float sum = 0.0f;
    for (int s = 0; s < numSpeakers; ++s)
    {
        outputGains[s] = std::sqrt (std::max (0.0f, outputGains[s]));
        sum += outputGains[s] * outputGains[s];
    }

    // Renormalise to unit power (sum of squares == 1); silence stays silence.
    if (sum > 1e-12f)
    {
        const float scale = 1.0f / std::sqrt (sum);
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] *= scale;
    }
}

} // namespace spatialcore
