#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <SpatialCore/Core/BinauralGains.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace spatialcore
{

struct LayoutContext
{
    const SpeakerLayout&             layout;
    const std::vector<VBAPTriplet>&  triplets;
    const float (*ambiDecodeMatrix)[MAX_SPEAKERS];
    int ambiNumSpeakers;
};

struct BinauralContext
{
    int    profileIndex;
    double sampleRate;
};

class SpatializationAlgorithm
{
public:
    virtual ~SpatializationAlgorithm() = default;

    virtual void computeGains(const SourcePosition& source,
                              const LayoutContext& ctx,
                              float* outputGains,
                              int numSpeakers) const = 0;

    virtual bool supportsBinauralDirect() const { return false; }
    virtual BinauralGains computeBinauralGains(const SourcePosition& /*source*/,
                                               const BinauralContext& /*ctx*/) const { return {}; }

    virtual bool supportsSurround() const { return true; }
    virtual bool supportsSHDomain() const { return false; }
    virtual juce::String getName() const = 0;
};

} // namespace spatialcore
