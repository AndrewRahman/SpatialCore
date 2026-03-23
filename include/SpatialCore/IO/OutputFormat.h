#pragma once

namespace spatialcore
{

enum class OutputFormat
{
    Binaural = 0,
    Stereo,
    Quad,
    Surround_5_0,
    Surround_5_1,
    Surround_7_0,
    Surround_7_1,
    Octaphonic,
    Atmos_5_1_2,
    Atmos_5_1_4,
    Atmos_7_1_2,
    Atmos_7_1_4,
    Atmos_7_1_6,
    Atmos_9_1_4,
    Atmos_9_1_6,
    SML_13_1,
    Ambi_FOA,
    Ambi_SOA,
    Ambi_HOA,
    Ambi_4OA,
    Ambi_5OA,
    Ambi_6OA,
    NumFormats
};

struct OutputFormatInfo
{
    OutputFormat format;
    const char*  name;
    const char*  shortName;
    int          requiredChannels;
    bool         hasLFE;
    bool         hasHeight;
    bool         isAmbisonicsOutput;
    int          ambiOrder;
    bool         isStereoVariant;
};

} // namespace spatialcore
