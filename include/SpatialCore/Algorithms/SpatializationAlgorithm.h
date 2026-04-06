#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <SpatialCore/Core/BinauralGains.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace spatialcore
{

//==============================================================================
// Context structs passed to spatialization algorithms
//==============================================================================
struct LayoutContext
{
    const SpeakerLayout&             layout;
    const std::vector<VBAPTriplet>&  triplets;   // empty for 2D-only layouts
    const float (*ambiDecodeMatrix)[MAX_SPEAKERS];  // Ambisonics decode matrix [speaker][channel]
    int ambiNumSpeakers;
};

struct BinauralContext
{
    int    profileIndex;
    double sampleRate;
    const BinauralProfile* profiles;             // pointer to the 5-profile array
};

//==============================================================================
// Abstract spatialization algorithm interface
// Shared across the Spatial Media Library plugin suite
//==============================================================================
class SpatializationAlgorithm
{
public:
    virtual ~SpatializationAlgorithm() = default;

    /** Compute speaker gains for a source position in the given layout. */
    virtual void computeGains (const SourcePosition& source,
                               const LayoutContext& ctx,
                               float* outputGains,
                               int numSpeakers) const = 0;

    /** Whether this algorithm can produce direct binaural output (bypassing speakers). */
    virtual bool supportsBinauralDirect() const { return false; }

    /** Compute direct binaural gains (only called if supportsBinauralDirect() is true). */
    virtual BinauralGains computeBinauralGains (const SourcePosition& /*source*/,
                                                const BinauralContext& /*ctx*/) const { return {}; }

    /** Whether this algorithm supports surround speaker output. */
    virtual bool supportsSurround() const { return true; }

    /** Whether this algorithm uses SH-domain (spherical harmonic) accumulation
        for HRTF convolution, rather than speaker-domain accumulation.
        When true, processBlock accumulates into SH buffers and uses SH-projected HRIRs.
        Only Ambisonics returns true; speaker-based algorithms (VBAP, VBIP, KNN) return false. */
    virtual bool supportsSHDomain() const { return false; }

    virtual juce::String getName() const = 0;
};

} // namespace spatialcore
