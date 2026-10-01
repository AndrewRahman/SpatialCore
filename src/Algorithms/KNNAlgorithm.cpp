#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <algorithm>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// KNNAlgorithm -- K-Nearest Neighbor panning (inverse-distance-squared)
//==============================================================================
void KNNAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                  float* outputGains, int numSpeakers) const
{
    for (int s = 0; s < numSpeakers; ++s)
        outputGains[s] = 0.0f;

    if (numSpeakers == 0) return;

    // Non-finite direction -> silence (D-06, D-19i). Without this the angular
    // distances are NaN and the gains come out NaN (RESEARCH F7). Kept in this
    // .cpp so a -ffast-math consumer cannot fold it away (F9).
    if (! std::isfinite (source.azimuthRad) || ! std::isfinite (source.elevationRad))
        return;

    constexpr int k = 3;
    int kClamped = std::min (k, numSpeakers);

    // Source direction as unit Cartesian vector
    float px = std::cos (source.elevationRad) * std::sin (source.azimuthRad);
    float py = std::cos (source.elevationRad) * std::cos (source.azimuthRad);
    float pz = std::sin (source.elevationRad);

    // Compute angular distances
    struct SpkDist { int index; float dist; };
    SpkDist dists[MAX_SPEAKERS];
    for (int s = 0; s < numSpeakers; ++s)
    {
        float sx = std::cos (ctx.layout.speakers[s].elevationRad) * std::sin (ctx.layout.speakers[s].azimuthRad);
        float sy = std::cos (ctx.layout.speakers[s].elevationRad) * std::cos (ctx.layout.speakers[s].azimuthRad);
        float sz = std::sin (ctx.layout.speakers[s].elevationRad);
        float dot = juce::jlimit (-1.0f, 1.0f, px * sx + py * sy + pz * sz);
        dists[s] = { s, std::acos (dot) };
    }

    // Partial sort to find K nearest
    std::partial_sort (dists, dists + kClamped, dists + numSpeakers,
                       [](const SpkDist& a, const SpkDist& b) { return a.dist < b.dist; });

    // Check if source is exactly at a speaker
    if (dists[0].dist < 1e-4f)
    {
        outputGains[dists[0].index] = 1.0f;
        return;
    }

    // Inverse-distance-squared weighting
    float totalWeight = 0.0f;
    float weights[MAX_SPEAKERS] = {};
    for (int i = 0; i < kClamped; ++i)
    {
        float w = 1.0f / (dists[i].dist * dists[i].dist + 1e-6f);
        weights[i] = w;
        totalWeight += w;
    }

    // Constant-power normalization
    float totalPower = 0.0f;
    for (int i = 0; i < kClamped; ++i)
    {
        float g = weights[i] / totalWeight;
        outputGains[dists[i].index] = g;
        totalPower += g * g;
    }

    if (totalPower > 1e-12f)
    {
        float scale = 1.0f / std::sqrt (totalPower);
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] *= scale;
    }
}

} // namespace spatialcore
