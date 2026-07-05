#pragma once

#include <vector>

namespace spatialcore
{

static constexpr int MAX_SOURCES  = 12;
static constexpr int MAX_SPEAKERS = 16;

// Forward declarations — SpeakerLayout.h includes this header for
// MAX_SOURCES/MAX_SPEAKERS, so Types.h cannot include SpeakerLayout.h back
// (circular). LayoutContext only needs these as reference/pointer members,
// which is legal with an incomplete type.
struct SpeakerLayout;
struct VBAPTriplet;

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
// Binaural gain result for one source position
//==============================================================================
struct BinauralGains
{
    float leftGain          = 0.0f;
    float rightGain         = 0.0f;
    float leftDelaySamples  = 0.0f;  // ITD: additional delay for left ear
    float rightDelaySamples = 0.0f;  // ITD: additional delay for right ear
};

//==============================================================================
// Source position for spatialization algorithm input
//==============================================================================
struct SourcePosition
{
    float azimuthRad;
    float elevationRad;
    float distance;
};

//==============================================================================
// Context structs passed to spatialization algorithms
//==============================================================================
struct LayoutContext
{
    const SpeakerLayout& layout;
    const std::vector<VBAPTriplet>& triplets;   // empty for 2D-only layouts
    const float (*ambiDecodeMatrix)[MAX_SPEAKERS]; // Ambisonics decode matrix [speaker][channel]
    int ambiNumSpeakers;
};

struct BinauralContext
{
    int profileIndex;
    double sampleRate;
    const BinauralProfile* profiles;             // pointer to the 5-profile array
};

} // namespace spatialcore
