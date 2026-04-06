#pragma once

#include <SpatialCore/Core/Types.h>

namespace spatialcore
{

struct VirtualSpeaker
{
    float azimuthRad   = 0.0f;
    float elevationRad = 0.0f;
};

struct VBAPTriplet
{
    int i = 0, j = 0, k = 0;
    float inv[3][3] = {};
};

struct SpeakerLayout
{
    int numSpeakers     = 0;
    int lfeChannelIndex = -1;
    int totalChannels   = 0;

    struct Speaker
    {
        float azimuthRad   = 0.0f;
        float elevationRad = 0.0f;
        int   channelIndex = 0;
    };

    Speaker speakers[MAX_SPEAKERS] = {};
};

namespace Layouts
{
    SpeakerLayout getQuad();
    SpeakerLayout get5_0();
    SpeakerLayout get5_1();
    SpeakerLayout get7_0();
    SpeakerLayout get7_1();
    SpeakerLayout get9_1();
    SpeakerLayout getOctaphonic();
    SpeakerLayout get5_1_2();
    SpeakerLayout get5_1_4();
    SpeakerLayout get7_1_2();
    SpeakerLayout get7_1_4();
    SpeakerLayout get7_1_6();
    SpeakerLayout get9_1_4();
    SpeakerLayout get9_1_6();
    SpeakerLayout getSML13_1();
    SpeakerLayout getVirtualBinaural16();
} // namespace Layouts

void buildVBAPTripletsForLayout(const SpeakerLayout& layout,
                                std::vector<VBAPTriplet>& triplets);

} // namespace spatialcore
