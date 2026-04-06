#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Vector Base Amplitude Panning (Pulkki 1997). 2D or 3D. */
class VBAPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "VBAP"; }
};

} // namespace spatialcore
