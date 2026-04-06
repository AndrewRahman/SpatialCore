#pragma once

#include <cmath>

namespace spatialcore
{
namespace DSP
{

//==============================================================================
// Soft Clipper (NaN-safe, preserves natural asymptotic curve for self-oscillation)
//==============================================================================
inline float softClip(float x)
{
    if (! std::isfinite(x))
        return 0.0f;

    const float threshold = 0.8f;
    if (x > threshold)
        return threshold + (x - threshold) / (1.0f + (x - threshold) * (x - threshold));
    if (x < -threshold)
        return -threshold + (x + threshold) / (1.0f + (x + threshold) * (x + threshold));
    return x;
}

//==============================================================================
// Output Limiter -- tanh-based soft ceiling for speaker protection
// C-infinity continuous (no derivative discontinuities), asymptotes to +/-threshold
//==============================================================================
inline float outputLimiter(float x)
{
    if (! std::isfinite(x))
        return 0.0f;
    const float threshold = 1.2589f;  // +2 dB
    return threshold * std::tanh(x / threshold);
}

} // namespace DSP
} // namespace spatialcore
