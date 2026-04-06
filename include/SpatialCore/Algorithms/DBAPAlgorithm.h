#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Distance-Based Amplitude Panning (Lossius et al., ICMC 2009).
    Computes speaker gains from Euclidean distances in Cartesian space.
    Ideal for irregular/non-standard speaker layouts where VBAP triangulation fails. */
class DBAPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "DBAP"; }
};

} // namespace spatialcore
