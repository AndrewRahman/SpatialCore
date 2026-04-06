#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Vector Base Intensity Panning -- VBAP with squared gains for tighter focus. */
class VBIPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "VBIP"; }
};

} // namespace spatialcore
