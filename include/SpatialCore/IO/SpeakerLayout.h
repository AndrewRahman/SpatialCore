#pragma once

#include <SpatialCore/Core/Types.h>
#include <vector>

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

// ITU-R BS.775 / BS.2051 / SMPTE ST 2098-1 standard speaker layouts, moved
// verbatim from Source/PluginProcessor.cpp's layoutDefs table + makeLayoutFromDef().
// Convention: 0 deg = front, positive azimuth = left, negative = right.
// LFE is tracked but excluded from spatialization.
enum LayoutID
{
    Quad, S5_0, S5_1, S7_0, S7_1, S9_1, S5_1_2, S5_1_4, S7_1_2, S7_1_4, S7_1_6,
    S9_1_4, S9_1_6, Octaphonic, SML13_1, NUM_LAYOUT_DEFS
};

const SpeakerLayout& getLayoutDef (LayoutID id);

// v1.0: Check if a speaker layout has height speakers (elevation > 1 degree)
bool layoutHasHeight (const SpeakerLayout& layout);

// Build 3D VBAP triplets for a speaker layout with height speakers (empty
// result for 2D-only layouts -- VBAP uses pair-wise panning, no triplets needed)
void buildVBAPTripletsForLayout (const SpeakerLayout& layout,
                                  std::vector<VBAPTriplet>& triplets);

} // namespace spatialcore
