#include <SpatialCore/IO/OutputFormatRegistry.h>

namespace spatialcore
{

static const OutputFormatInfo kFormatTable[] = {
    //                                                              ch  LFE  height ambi ord  stereo
    // --- Binaural (HRTF head model) --- default, listed first ---
    { OutputFormat::Binaural,       "Binaural",         "Bin",    2, false, false, false, 0, false },
    // --- Stereo (mode selected by algorithm param) ---
    { OutputFormat::Stereo,         "Stereo",           "St",     2, false, false, false, 0, true  },
    // --- Surround (ascending channel count) ---
    { OutputFormat::Quad,           "Quadraphonic",     "Quad",   4, false, false, false, 0, false },
    { OutputFormat::Surround_5_0,   "5.0 Surround",    "5.0",    5, false, false, false, 0, false },
    { OutputFormat::Surround_5_1,   "5.1 Surround",    "5.1",    6, true,  false, false, 0, false },
    { OutputFormat::Surround_7_0,   "7.0 Surround",    "7.0",    7, false, false, false, 0, false },
    { OutputFormat::Surround_7_1,   "7.1 Surround",    "7.1",    8, true,  false, false, 0, false },
    // --- 9.1 Surround (ITU-R BS.2051 System H --- ear level only, no height) ---
    { OutputFormat::Surround_9_1,   "9.1 Surround",    "9.1",   10, true,  false, false, 0, false },
    // --- Octaphonic ---
    { OutputFormat::Octaphonic,     "Octaphonic",       "Oct",    8, false, false, false, 0, false },
    // --- Atmos / Immersive (ascending channel count) ---
    { OutputFormat::Atmos_5_1_2,    "5.1.2 Atmos",     "5.1.2",  8, true,  true,  false, 0, false },
    { OutputFormat::Atmos_5_1_4,    "5.1.4 Atmos",     "5.1.4", 10, true,  true,  false, 0, false },
    { OutputFormat::Atmos_7_1_2,    "7.1.2 Atmos",     "7.1.2", 10, true,  true,  false, 0, false },
    { OutputFormat::Atmos_7_1_4,    "7.1.4 Atmos",     "7.1.4", 12, true,  true,  false, 0, false },
    { OutputFormat::Atmos_7_1_6,    "7.1.6 Atmos",     "7.1.6", 14, true,  true,  false, 0, false },
    { OutputFormat::Atmos_9_1_4,    "9.1.4 Atmos",     "9.1.4", 14, true,  true,  false, 0, false },
    { OutputFormat::Atmos_9_1_6,    "9.1.6 Atmos",     "9.1.6", 16, true,  true,  false, 0, false },
    // --- SML (Spatial Media Lab custom room) ---
    { OutputFormat::SML_13_1,       "SpatialMediaLab 13.1", "SML", 14, true,  true,  false, 0, false },
    // --- Ambisonics output (AmbiX ACN/SN3D encoding) ---
    { OutputFormat::Ambi_FOA,       "1st Order Ambi",  "FOA",    4, false, false, true,  1, false },
    { OutputFormat::Ambi_SOA,       "2nd Order Ambi",  "SOA",    9, false, false, true,  2, false },
    { OutputFormat::Ambi_HOA,       "3rd Order Ambi",  "HOA",   16, false, false, true,  3, false },
    { OutputFormat::Ambi_4OA,       "4th Order Ambi",  "4OA",   25, false, false, true,  4, false },
    { OutputFormat::Ambi_5OA,       "5th Order Ambi",  "5OA",   36, false, false, true,  5, false },
    { OutputFormat::Ambi_6OA,       "6th Order Ambi",  "6OA",   49, false, false, true,  6, false },
};

static SpeakerLayout sLayouts[static_cast<int>(OutputFormat::NumFormats)] = {};
static std::vector<VBAPTriplet> sTriplets[static_cast<int>(OutputFormat::NumFormats)] = {};
static bool sInitialized = false;

void OutputFormatRegistry::initLayouts()
{
    if (sInitialized) return;
    sInitialized = true;

    sLayouts[static_cast<int>(OutputFormat::Quad)]        = Layouts::getQuad();
    sLayouts[static_cast<int>(OutputFormat::Surround_5_0)]= Layouts::get5_0();
    sLayouts[static_cast<int>(OutputFormat::Surround_5_1)]= Layouts::get5_1();
    sLayouts[static_cast<int>(OutputFormat::Surround_7_0)]= Layouts::get7_0();
    sLayouts[static_cast<int>(OutputFormat::Surround_7_1)]= Layouts::get7_1();
    sLayouts[static_cast<int>(OutputFormat::Surround_9_1)]= Layouts::get9_1();
    sLayouts[static_cast<int>(OutputFormat::Octaphonic)]  = Layouts::getOctaphonic();
    sLayouts[static_cast<int>(OutputFormat::Atmos_5_1_2)] = Layouts::get5_1_2();
    sLayouts[static_cast<int>(OutputFormat::Atmos_5_1_4)] = Layouts::get5_1_4();
    sLayouts[static_cast<int>(OutputFormat::Atmos_7_1_2)] = Layouts::get7_1_2();
    sLayouts[static_cast<int>(OutputFormat::Atmos_7_1_4)] = Layouts::get7_1_4();
    sLayouts[static_cast<int>(OutputFormat::Atmos_7_1_6)] = Layouts::get7_1_6();
    sLayouts[static_cast<int>(OutputFormat::Atmos_9_1_4)] = Layouts::get9_1_4();
    sLayouts[static_cast<int>(OutputFormat::Atmos_9_1_6)] = Layouts::get9_1_6();
    sLayouts[static_cast<int>(OutputFormat::SML_13_1)]    = Layouts::getSML13_1();
    sLayouts[static_cast<int>(OutputFormat::Binaural)]    = Layouts::getVirtualBinaural16();

    // Build VBAP triplets for layouts with height speakers
    for (int i = 0; i < static_cast<int>(OutputFormat::NumFormats); ++i)
        buildVBAPTripletsForLayout(sLayouts[i], sTriplets[i]);
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

const char* OutputFormatRegistry::getDisplayName(OutputFormat format)
{
    int idx = static_cast<int>(format);
    if (idx >= 0 && idx < static_cast<int>(OutputFormat::NumFormats))
        return kFormatTable[idx].name;
    return "Unknown";
}

} // namespace spatialcore
