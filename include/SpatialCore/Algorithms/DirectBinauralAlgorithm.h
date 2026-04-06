#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Woodworth ITD+ILD binaural model -- used internally for "Simple (Low CPU)" profile.
    Not in the user-facing algorithm dropdown; kept for Woodworth speaker cache gains. */
class DirectBinauralAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    bool supportsBinauralDirect() const override { return true; }
    BinauralGains computeBinauralGains (const SourcePosition& source,
                                        const BinauralContext& ctx) const override;
    bool supportsSurround() const override { return false; }
    juce::String getName() const override { return "Direct Binaural"; }
};

} // namespace spatialcore
