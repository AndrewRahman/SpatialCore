#pragma once

namespace spatialcore
{

struct BinauralGains
{
    float leftGain          = 0.0f;
    float rightGain         = 0.0f;
    float leftDelaySamples  = 0.0f;
    float rightDelaySamples = 0.0f;
};

} // namespace spatialcore
