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
// NOTE: layoutHasHeight() is declared (non-inline) in SpeakerLayout.h and
// defined once in SpeakerLayout.cpp (added in 08-05). A duplicate `inline`
// definition used to live here too (from 08-02, before SpeakerLayout.cpp's
// real implementation existed) -- MSVC's linker caught it as LNK2005/LNK1169
// multiply-defined-symbol once both TUs were linked into the same binary.
// Removed; SpeakerLayout.h is already #included above, so callers of this
// header get the canonical declaration automatically.
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

//==============================================================================
// SPATIAL FRAMEWORK: Utility DSP (reusable by any SML plugin)
// Consolidated verbatim from Source/PluginProcessor.h:1083-1104 (softClip,
// outputLimiter) and the distance-attenuation formula duplicated at
// Source/PluginProcessor.cpp:3091 and :4500 (verified byte-identical before
// consolidation, per D-09 / RESEARCH.md Pitfall 5 — see 08-02-SUMMARY.md).
//==============================================================================

// Soft Clipper helper (NaN-safe, preserves natural asymptotic curve for self-oscillation)
inline float softClip (float x)
{
    if (! std::isfinite (x))
        return 0.0f;

    const float threshold = 0.8f;
    if (x > threshold)
        return threshold + (x - threshold) / (1.0f + (x - threshold) * (x - threshold));
    if (x < -threshold)
        return -threshold + (x + threshold) / (1.0f + (x + threshold) * (x + threshold));
    return x;
}

// Output Limiter -- tanh-based soft ceiling for speaker protection during self-oscillation
// C-infinity continuous (no derivative discontinuities), asymptotes to +/-threshold
inline float outputLimiter (float x)
{
    if (! std::isfinite (x))
        return 0.0f;
    const float threshold = 1.2589f;  // +2 dB
    return threshold * std::tanh (x / threshold);
}

// Distance attenuation (inverse-distance law, clamped) -- consolidated from the
// two verified byte-identical occurrences in PluginProcessor.cpp (:3091, :4500)
inline float distanceAttenuation (float distance)
{
    return 1.0f / std::max (0.1f, distance * 4.0f + 0.25f);
}

} // namespace spatialcore
