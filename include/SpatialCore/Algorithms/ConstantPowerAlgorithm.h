#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Constant Power Panning -- cosine-distance all-speaker weighting.
    Activates all speakers within 90 degrees of the source with natural
    cosine rolloff, constant-power normalized. Smooth, wide image. */
class ConstantPowerAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "Constant Power"; }
};

} // namespace spatialcore
