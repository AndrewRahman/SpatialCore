#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Vector Base Intensity Panning (Pernaux, Boussard & Jot, DAFx-98 sec. 2.2.2).

    The VBAP gains of the active pair (2D) or triplet (3D) are raised to the
    exponent 1/2 and renormalised so the sum of squares is 1, i.e.
    g_i = sqrt (G_i / sum G_j) with G = L^-1 p. This aims the energy vector rE at
    the source, where VBAP aims the velocity vector rV, so VBIP is wider than VBAP
    between speakers and has unit power everywhere.

    Single band: the paper pairs VBAP below 700 Hz with VBIP above 700 Hz.
    SpatialCore implements only the VBIP half, at all frequencies, because a
    per-object crossover cannot be expressed through the frozen computeGains
    interface (DR-2/DR-7). Dual-band VBAP/VBIP is tracked in
    AndrewRahman/SpatialCore#20 (D-15).

    Below the horizon on height layouts the transform operates on the
    EAR-downmixed VBAP vector (D-04), a defined extension of the paper. */
class VBIPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "VBIP"; }
};

} // namespace spatialcore
