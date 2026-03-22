#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{
class DirectBinauralAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains(const SourcePosition& source, const LayoutContext& ctx,
                      float* outputGains, int numSpeakers) const override;
    bool supportsBinauralDirect() const override { return true; }
    bool supportsSurround() const override { return false; }
    BinauralGains computeBinauralGains(const SourcePosition& source,
                                       const BinauralContext& ctx) const override;
    juce::String getName() const override { return "Direct Binaural"; }
};
} // namespace spatialcore
