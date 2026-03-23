#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{
class AmbisonicsAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains(const SourcePosition& source, const LayoutContext& ctx,
                      float* outputGains, int numSpeakers) const override;
    bool supportsSHDomain() const override { return true; }
    juce::String getName() const override { return "Ambisonics"; }
};
} // namespace spatialcore
