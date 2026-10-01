#pragma once
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>

namespace spatialcore
{

/** Distance-Based Amplitude Panning (Lossius, Baltazar & de la Hogue, ICMC 2009),
    documented as implemented (D-16).

    Speakers sit on the unit sphere and the source at `distance` times its unit
    direction. Each speaker's weight is 1 / d^2, with d^2 the squared Euclidean
    source-to-speaker distance clamped at 0.001, and the weights are used
    directly as amplitudes and then normalised to unit power. In the paper's
    terms (v_i = k / d_i^a, a = R / (20 log10 2)) that is rolloff exponent a = 2,
    i.e. an effective rolloff R = 20 log10(2) x 2 = 12.04 dB per doubling of
    distance (the paper's default is 6 dB).

    Not implemented: no spatial blur (r_s = 0), no user rolloff parameter, and
    no convex-hull projection for sources outside the speaker hull. These are
    deferred (Phase 2 CONTEXT, Deferred Ideas; relates to
    AndrewRahman/SpatialCore#10). Because of the d^2 clamp, a source exactly on
    a speaker gets unity on it only to about 1e-3.

    Useful for irregular/non-standard layouts where VBAP triangulation is
    ill-conditioned. */
class DBAPAlgorithm : public SpatializationAlgorithm
{
public:
    void computeGains (const SourcePosition& source, const LayoutContext& ctx,
                       float* outputGains, int numSpeakers) const override;
    juce::String getName() const override { return "DBAP"; }
};

} // namespace spatialcore
