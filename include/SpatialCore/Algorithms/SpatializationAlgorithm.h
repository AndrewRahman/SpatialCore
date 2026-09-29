#pragma once
#include <juce_core/juce_core.h>
#include <SpatialCore/Core/Types.h>

namespace spatialcore
{

//==============================================================================
// Abstract spatialization algorithm interface
// Shared across the Spatial Media Library plugin suite.
// FROZEN (D-02) — moved verbatim from Source/PluginProcessor.h:104-132.
// Do not add, remove, or change any virtual method signature.
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
