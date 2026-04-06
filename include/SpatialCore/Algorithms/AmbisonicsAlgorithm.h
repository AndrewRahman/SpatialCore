#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** 3rd-order Ambisonics (ACN/SN3D) with max-rE weighting.
    Uses SH-domain HRTF convolution for binaural output. */
class AmbisonicsAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    bool supportsSHDomain() const override { return true; }
    juce::String getName() const override { return "Ambisonics (HOA)"; }
};

} // namespace spatialcore
