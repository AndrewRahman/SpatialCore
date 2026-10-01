#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Multiple-Direction Amplitude Panning (Pulkki, "Uniform spreading of amplitude
    panned virtual sources", IEEE WASPAA 1999).
    Creates source spread by rendering multiple VBAP sub-sources on a ring
    around the main direction. Produces wider, more stable spatial images. */
class MDAPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "MDAP"; }
};

} // namespace spatialcore
