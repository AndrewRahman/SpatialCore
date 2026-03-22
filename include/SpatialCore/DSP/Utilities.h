#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace spatialcore
{
namespace DSP
{

inline float softClip(float x)
{
    if (!std::isfinite(x))
        return 0.0f;

    const float threshold = 0.8f;
    if (x > threshold)
        return threshold + (x - threshold) / (1.0f + (x - threshold) * (x - threshold));
    if (x < -threshold)
        return -threshold + (x + threshold) / (1.0f + (x + threshold) * (x + threshold));
    return x;
}

inline float outputLimiter(float x)
{
    if (!std::isfinite(x))
        return 0.0f;
    const float ceiling = 1.2589f;  // +2 dB hard ceiling
    if (x > ceiling)
        return ceiling;
    if (x < -ceiling)
        return -ceiling;
    return x;
}

} // namespace DSP
} // namespace spatialcore
