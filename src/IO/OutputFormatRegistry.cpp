#include <SpatialCore/IO/OutputFormatRegistry.h>

namespace spatialcore
{

//==============================================================================
// Output format registry -- single source of truth for all speaker layouts
// Moved verbatim from OpenSpatialDelayProcessor::outputFormatRegistry
// (Source/PluginProcessor.cpp) -- Reusable across Spatial Media Library plugins
//==============================================================================
const std::array<OutputFormatInfo, NUM_OUTPUT_FORMATS> OutputFormatRegistry::table = {{
    //                                                              ch  LFE  height ambi ord  stereo
    // --- Binaural (HRTF head model) -- default, listed first ---
    { OutputFormat::Binaural,       "Binaural",         "Bin",    2, false, false, false, 0, false },
    // --- Stereo (mode selected by algorithm param indices 6-10) ---
    { OutputFormat::Stereo,         "Stereo",           "St",     2, false, false, false, 0, true  },
    // --- Surround (ascending channel count) ---
    { OutputFormat::Quad,           "Quadraphonic",     "Quad",   4, false, false, false, 0, false },
    { OutputFormat::Surround5_0,    "5.0 Surround",     "5.0",    5, false, false, false, 0, false },
    { OutputFormat::Surround5_1,    "5.1 Surround",     "5.1",    6, true,  false, false, 0, false },
    { OutputFormat::Surround7_0,    "7.0 Surround",     "7.0",    7, false, false, false, 0, false },
    { OutputFormat::Surround7_1,    "7.1 Surround",     "7.1",    8, true,  false, false, 0, false },
    // --- 9.1 Surround (ITU-R BS.2051 System H -- ear level only, no height) ---
    { OutputFormat::Surround9_1,    "9.1 Surround",     "9.1",   10, true,  false, false, 0, false },
    // --- Octaphonic ---
    { OutputFormat::Octaphonic,     "Octaphonic",       "Oct",    8, false, false, false, 0, false },
    // --- Atmos / Immersive (ascending channel count) ---
    { OutputFormat::Surround5_1_2,  "5.1.2 Atmos",      "5.1.2",  8, true,  true,  false, 0, false },
    { OutputFormat::Surround5_1_4,  "5.1.4 Atmos",      "5.1.4", 10, true,  true,  false, 0, false },
    { OutputFormat::Surround7_1_2,  "7.1.2 Atmos",      "7.1.2", 10, true,  true,  false, 0, false },
    { OutputFormat::Surround7_1_4,  "7.1.4 Atmos",      "7.1.4", 12, true,  true,  false, 0, false },
    { OutputFormat::Surround7_1_6,  "7.1.6 Atmos",      "7.1.6", 14, true,  true,  false, 0, false },
    { OutputFormat::Surround9_1_4,  "9.1.4 Atmos",      "9.1.4", 14, true,  true,  false, 0, false },
    { OutputFormat::Surround9_1_6,  "9.1.6 Atmos",      "9.1.6", 16, true,  true,  false, 0, false },
    // --- SML (Spatial Media Lab custom room) ---
    { OutputFormat::SurroundSML13_1,"SpatialMediaLab 13.1", "SML", 14, true,  true,  false, 0, false },
    // --- Ambisonics output (AmbiX ACN/SN3D encoding) ---
    { OutputFormat::AmbisonicsFOA,  "1st Order Ambi",   "FOA",    4, false, false, true,  1, false },
    { OutputFormat::AmbisonicsSOA,  "2nd Order Ambi",   "SOA",    9, false, false, true,  2, false },
    { OutputFormat::AmbisonicsHOA,  "3rd Order Ambi",   "HOA",   16, false, false, true,  3, false },
    { OutputFormat::Ambisonics4OA,  "4th Order Ambi",   "4OA",   25, false, false, true,  4, false },
    { OutputFormat::Ambisonics5OA,  "5th Order Ambi",   "5OA",   36, false, false, true,  5, false },
    { OutputFormat::Ambisonics6OA,  "6th Order Ambi",   "6OA",   49, false, false, true,  6, false },
}};

const OutputFormatInfo& OutputFormatRegistry::getInfo (OutputFormat format)
{
    return table[static_cast<size_t> (format)];
}

const char* OutputFormatRegistry::getDisplayName (OutputFormat format)
{
    for (const auto& info : table)
        if (info.format == format)
            return info.name;
    return "Unknown";
}

} // namespace spatialcore
