#pragma once

namespace spatialcore
{

// Output formats — Binaural, Stereo, Surround, Octaphonic, Atmos, SML, Ambisonics
// 1 Binaural + 1 Stereo + 15 Surround + 6 Ambisonics = 23 total
// Stereo mode (Equal Power, VBAP, XY, MS, Blumlein) selected via algorithm parameter
enum class OutputFormat
{
    // Binaural (HRTF head model) -- default
    Binaural = 0,
    // Stereo (mode selected by algorithm indices 7-11: kAlgorithmIndexEqualPower .. kAlgorithmIndexBlumlein, RenderEngine.h)
    Stereo,
    // Surround (ascending channel count)
    Quad, Surround5_0, Surround5_1, Surround7_0, Surround7_1,
    // 9.1 Surround (ITU-R BS.2051 System H -- ear level only, no height)
    Surround9_1,
    // Octaphonic
    Octaphonic,
    // Atmos / Immersive (ascending channel count)
    Surround5_1_2, Surround5_1_4, Surround7_1_2,
    Surround7_1_4, Surround7_1_6, Surround9_1_4, Surround9_1_6,
    SurroundSML13_1,  // SML Multi-Use Room (13 speakers + LFE)
    // Ambisonics output (AmbiX ACN/SN3D)
    AmbisonicsFOA, AmbisonicsSOA, AmbisonicsHOA,
    Ambisonics4OA, Ambisonics5OA, Ambisonics6OA
};

// Output format registry entry -- single source of truth for all supported formats
// Reusable across Spatial Media Library plugins
struct OutputFormatInfo
{
    OutputFormat format;
    const char* name;           // UI display name (e.g., "7.1.4 Atmos")
    const char* shortName;      // Compact name (e.g., "7.1.4")
    int requiredChannels;       // Minimum bus channels needed
    bool hasLFE;
    bool hasHeight;
    bool isAmbisonicsOutput;    // true for FOA/SOA/HOA output encoding
    int  ambiOrder;             // 0 for non-ambi, 1-6 for Ambisonics output
    bool isStereoVariant;       // true for Stereo (single entry, mode via algorithm param)
};

} // namespace spatialcore
