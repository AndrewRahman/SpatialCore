#pragma once

#include <juce_core/juce_core.h>

namespace spatialcore
{

static constexpr int MAX_SOURCES  = 12;
static constexpr int MAX_SPEAKERS = 16;

//==============================================================================
// Binaural profile: defines virtual head characteristics for simplified HRTF
//==============================================================================
struct BinauralProfile
{
    float headRadius;   // meters
    float ildScale;     // ILD multiplier
    float shadowFreqHz; // head shadow cutoff frequency
    const char* name;
};

//==============================================================================
// Per-object spatial state
//==============================================================================
struct ObjectState
{
    float azimuthDeg   = 0.0f;
    float elevationDeg = 0.0f;
    float distance     = 0.5f;
    bool  enabled      = false;
};

//==============================================================================
// Per-object trajectory state (for editor visualization)
//==============================================================================
struct TrajectoryState
{
    float originAzDeg  = 0.0f;   // Base/origin position (captured at shape change)
    float originElDeg  = 0.0f;
    float originDist   = 0.5f;
    int   shape        = 0;      // 0 = None, 1+ = active trajectory
    float phase        = 0.0f;   // 0..1 animation progress
    bool  reverse      = false;
    float randomTime   = 0.0f;   // Random trajectory: current time accumulator
};

} // namespace spatialcore
