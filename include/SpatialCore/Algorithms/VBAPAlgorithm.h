#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{
class VBAPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains(const SourcePosition& source, const LayoutContext& ctx,
                      float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "VBAP"; }
};
} // namespace spatialcore
