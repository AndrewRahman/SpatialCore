#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <juce_core/juce_core.h>
#include <vector>

// NOTE: LayoutContext and BinauralContext now live in
// <SpatialCore/Core/Types.h> (consolidated in Phase 8 Plan 08-02 — see
// .planning/phases/08-spatialcore-dsp-extraction/08-02-PLAN.md). They were
// previously defined in this file; kept here as a comment for traceability
// since this header is not itself listed in 08-02's files_modified (that is
// 08-03's scope), but the duplicate struct definitions had to be removed to
// avoid a compile error now that Types.h defines them.

namespace spatialcore
{

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
