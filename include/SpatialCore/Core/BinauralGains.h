#pragma once

namespace spatialcore
{

//==============================================================================
// Binaural gain result for one source position
//==============================================================================
struct BinauralGains
{
    float leftGain          = 0.0f;
    float rightGain         = 0.0f;
    float leftDelaySamples  = 0.0f;  // ITD: additional delay for left ear
    float rightDelaySamples = 0.0f;  // ITD: additional delay for right ear
};

} // namespace spatialcore
