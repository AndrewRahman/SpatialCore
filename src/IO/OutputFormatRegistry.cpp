#include <SpatialCore/IO/OutputFormatRegistry.h>

namespace spatialcore
{

static const OutputFormatInfo kFormatTable[] = {
    { OutputFormat::Binaural,      "Binaural HRTF",    "Binaural",  2,  false, false, false, 0, false },
    { OutputFormat::Stereo,        "Stereo",           "Stereo",    2,  false, false, false, 0, true  },
    { OutputFormat::Quad,          "Quad",             "Quad",      4,  false, false, false, 0, false },
    { OutputFormat::Surround_5_0,  "5.0 Surround",    "5.0",       5,  false, false, false, 0, false },
    { OutputFormat::Surround_5_1,  "5.1 Surround",    "5.1",       6,  true,  false, false, 0, false },
    { OutputFormat::Surround_7_0,  "7.0 Surround",    "7.0",       7,  false, false, false, 0, false },
    { OutputFormat::Surround_7_1,  "7.1 Surround",    "7.1",       8,  true,  false, false, 0, false },
    { OutputFormat::Octaphonic,    "Octaphonic",       "8ch",       8,  false, false, false, 0, false },
    { OutputFormat::Atmos_5_1_2,   "5.1.2 Atmos",     "5.1.2",     8,  true,  true,  false, 0, false },
    { OutputFormat::Atmos_5_1_4,   "5.1.4 Atmos",     "5.1.4",     10, true,  true,  false, 0, false },
    { OutputFormat::Atmos_7_1_2,   "7.1.2 Atmos",     "7.1.2",     10, true,  true,  false, 0, false },
    { OutputFormat::Atmos_7_1_4,   "7.1.4 Atmos",     "7.1.4",     12, true,  true,  false, 0, false },
    { OutputFormat::Atmos_7_1_6,   "7.1.6 Atmos",     "7.1.6",     14, true,  true,  false, 0, false },
    { OutputFormat::Atmos_9_1_4,   "9.1.4 Atmos",     "9.1.4",     14, true,  true,  false, 0, false },
    { OutputFormat::Atmos_9_1_6,   "9.1.6 Atmos",     "9.1.6",     16, true,  true,  false, 0, false },
    { OutputFormat::SML_13_1,      "SML 13.1",        "13.1",      14, true,  true,  false, 0, false },
    { OutputFormat::Ambi_FOA,      "1st Order Ambi",   "FOA",       4,  false, true,  true,  1, false },
    { OutputFormat::Ambi_SOA,      "2nd Order Ambi",   "SOA",       9,  false, true,  true,  2, false },
    { OutputFormat::Ambi_HOA,      "3rd Order Ambi",   "HOA",      16,  false, true,  true,  3, false },
    { OutputFormat::Ambi_4OA,      "4th Order Ambi",   "4OA",      25,  false, true,  true,  4, false },
    { OutputFormat::Ambi_5OA,      "5th Order Ambi",   "5OA",      36,  false, true,  true,  5, false },
    { OutputFormat::Ambi_6OA,      "6th Order Ambi",   "6OA",      49,  false, true,  true,  6, false },
};

static SpeakerLayout sLayouts[static_cast<int>(OutputFormat::NumFormats)] = {};
static std::vector<VBAPTriplet> sTriplets[static_cast<int>(OutputFormat::NumFormats)] = {};
static bool sInitialized = false;

void OutputFormatRegistry::initLayouts()
{
    if (sInitialized) return;
    sInitialized = true;
    // Layouts will be populated during Phase 1 extraction
}

const OutputFormatInfo& OutputFormatRegistry::getInfo(OutputFormat format)
{
    return kFormatTable[static_cast<int>(format)];
}

const OutputFormatInfo* OutputFormatRegistry::getAllFormats()
{
    return kFormatTable;
}

int OutputFormatRegistry::getNumFormats()
{
    return static_cast<int>(OutputFormat::NumFormats);
}

OutputFormat OutputFormatRegistry::detectFromChannelCount(int numChannels)
{
    for (int i = 0; i < static_cast<int>(OutputFormat::NumFormats); ++i)
        if (kFormatTable[i].requiredChannels == numChannels)
            return kFormatTable[i].format;
    return OutputFormat::Stereo;
}

const SpeakerLayout& OutputFormatRegistry::getLayoutForFormat(OutputFormat format)
{
    initLayouts();
    return sLayouts[static_cast<int>(format)];
}

const std::vector<VBAPTriplet>& OutputFormatRegistry::getTripletsForFormat(OutputFormat format)
{
    initLayouts();
    return sTriplets[static_cast<int>(format)];
}

} // namespace spatialcore
