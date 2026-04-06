#pragma once

namespace spatialcore
{

enum class OutputFormat
{
    // Binaural (HRTF head model) -- default
    Binaural = 0,
    // Stereo (mode selected by algorithm param)
    Stereo,
    // Surround (ascending channel count)
    Quad, Surround_5_0, Surround_5_1, Surround_7_0, Surround_7_1,
    // 9.1 Surround (ITU-R BS.2051 System H -- ear level only, no height)
    Surround_9_1,
    // Octaphonic
    Octaphonic,
    // Atmos / Immersive (ascending channel count)
    Atmos_5_1_2, Atmos_5_1_4, Atmos_7_1_2,
    Atmos_7_1_4, Atmos_7_1_6, Atmos_9_1_4, Atmos_9_1_6,
    SML_13_1,  // SML Multi-Use Room (13 speakers + LFE)
    // Ambisonics output (AmbiX ACN/SN3D)
    Ambi_FOA, Ambi_SOA, Ambi_HOA,
    Ambi_4OA, Ambi_5OA, Ambi_6OA,
    NumFormats
};

struct OutputFormatInfo
{
    OutputFormat format;
    const char*  name;           // UI display name (e.g., "7.1.4 Atmos")
    const char*  shortName;      // Compact name (e.g., "7.1.4")
    int          requiredChannels;
    bool         hasLFE;
    bool         hasHeight;
    bool         isAmbisonicsOutput;
    int          ambiOrder;      // 0 for non-ambi, 1-6 for Ambisonics output
    bool         isStereoVariant;
};

} // namespace spatialcore
