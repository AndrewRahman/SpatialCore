#pragma once

#include <juce_core/juce_core.h>
#include <SpatialCore/Core/Types.h>
#include <SpatialCore/IO/SpeakerLayout.h>

namespace spatialcore
{

//==============================================================================
// Degree-to-radian conversion
//==============================================================================
inline constexpr float degToRad (float deg) { return deg * juce::MathConstants<float>::pi / 180.0f; }

//==============================================================================
// Cartesian->Polar conversion per ITU-R BS.2127-0
// Converts ADM-OSC Cartesian (x,y,z) to polar (azimuth, elevation, distance)
//==============================================================================
inline void cartesianToPolar (float x, float y, float z,
                              float& azDeg, float& elDeg, float& dist)
{
    azDeg = std::atan2 (-x, y) * (180.0f / juce::MathConstants<float>::pi);
    float r = std::sqrt (x * x + y * y);
    elDeg = std::atan2 (z, r) * (180.0f / juce::MathConstants<float>::pi);
    dist = juce::jlimit (0.0f, 1.0f, std::sqrt (x * x + y * y + z * z));
}

//==============================================================================
// v1.0: Check if a speaker layout has height speakers (elevation > 1 degree)
//==============================================================================
inline bool layoutHasHeight (const SpeakerLayout& layout)
{
    for (int s = 0; s < layout.numSpeakers; ++s)
        if (std::abs (layout.speakers[s].elevationRad) > 0.0175f)  // ~1 degree
            return true;
    return false;
}

//==============================================================================
// v1.0: 3D nearest-speaker fallback -- used when triplets are empty on a 3D layout.
// Prevents 2D fallback from routing signal to height speakers for horizontal sources.
//==============================================================================
inline void nearestSpeaker3DFallback (const SpeakerLayout& layout,
                                      float azimuthRad, float elevationRad,
                                      float* outGains, int numSpeakers)
{
    for (int s = 0; s < numSpeakers; ++s)
        outGains[s] = 0.0f;

    float px = std::cos (elevationRad) * std::sin (azimuthRad);
    float py = std::cos (elevationRad) * std::cos (azimuthRad);
    float pz = std::sin (elevationRad);

    float bestDot = -2.0f;
    int bestSpeaker = 0;
    for (int s = 0; s < numSpeakers; ++s)
    {
        float sx = std::cos (layout.speakers[s].elevationRad) * std::sin (layout.speakers[s].azimuthRad);
        float sy = std::cos (layout.speakers[s].elevationRad) * std::cos (layout.speakers[s].azimuthRad);
        float sz = std::sin (layout.speakers[s].elevationRad);
        float dot = px * sx + py * sy + pz * sz;
        if (dot > bestDot)
        {
            bestDot = dot;
            bestSpeaker = s;
        }
    }
    outGains[bestSpeaker] = 1.0f;
}

//==============================================================================
// Forward declarations for functions defined in SpatialMath.cpp
//==============================================================================
float evalSH (int acnIndex, float azimuthRad, float elevationRad);
void computeVBAPGains2D (const SpeakerLayout& layout, float azimuthRad, float* outGains);
void computeVBAPGains3D (const SpeakerLayout& layout, const std::vector<VBAPTriplet>& triplets,
                         float azimuthRad, float elevationRad, float* outGains);

} // namespace spatialcore
