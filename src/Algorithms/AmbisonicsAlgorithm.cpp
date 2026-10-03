#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
#include <SpatialCore/Core/SpatialMath.h>
#include <algorithm>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// AmbisonicsAlgorithm -- 3rd-order HOA (ACN/SN3D) with max-rE weighting
//==============================================================================
void AmbisonicsAlgorithm::computeGains (const SourcePosition& source, const LayoutContext& ctx,
                                         float* outputGains, int numSpeakers) const
{
    constexpr int HOA_CH = 16;  // (3+1)^2 = 16

    // Max-rE weights per order (Zotter & Frank 2012)
    static const float maxrE[4] = {
        1.0f,
        std::cos (juce::MathConstants<float>::pi / 8.0f),
        std::cos (2.0f * juce::MathConstants<float>::pi / 8.0f),
        std::cos (3.0f * juce::MathConstants<float>::pi / 8.0f),
    };
    auto acnToOrder = [](int acn) -> int {
        if (acn < 1) return 0; if (acn < 4) return 1;
        if (acn < 9) return 2; return 3;
    };

    // Non-finite direction -> silence (D-06, IN-01). Explicit, so it no longer
    // rests on std::max (0.0f, NaN) returning its first argument.
    if (! std::isfinite (source.azimuthRad) || ! std::isfinite (source.elevationRad))
    {
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] = 0.0f;
        return;
    }

    // Step 1: SH encode with max-rE weighting
    float coeffs[HOA_CH];
    for (int c = 0; c < HOA_CH; ++c)
        coeffs[c] = evalSH (c, source.azimuthRad, source.elevationRad) * maxrE[acnToOrder (c)];

    // Step 2: Decode via matrix multiply: gain[s] = sum_c D[s][c] * coeffs[c]
    float totalPower = 0.0f;
    for (int s = 0; s < numSpeakers; ++s)
    {
        float gain = 0.0f;
        for (int c = 0; c < HOA_CH; ++c)
            gain += ctx.ambiDecodeMatrix[s][c] * coeffs[c];

        outputGains[s] = std::max (0.0f, gain);
        totalPower += outputGains[s] * outputGains[s];
    }

    // Step 3: Constant-power normalization
    if (totalPower > 1e-12f)
    {
        float scale = 1.0f / std::sqrt (totalPower);
        for (int s = 0; s < numSpeakers; ++s)
            outputGains[s] *= scale;
    }
}

} // namespace spatialcore
