#pragma once

#include <SpatialCore/IO/OutputFormat.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <vector>

namespace spatialcore
{

class OutputFormatRegistry
{
public:
    static const OutputFormatInfo& getInfo(OutputFormat format);
    static const OutputFormatInfo* getAllFormats();
    static int getNumFormats();

    static OutputFormat detectFromChannelCount(int numChannels);
    static const SpeakerLayout& getLayoutForFormat(OutputFormat format);
    static const std::vector<VBAPTriplet>& getTripletsForFormat(OutputFormat format);

    static const char* getDisplayName(OutputFormat format);

private:
    static void initLayouts();
};

} // namespace spatialcore
