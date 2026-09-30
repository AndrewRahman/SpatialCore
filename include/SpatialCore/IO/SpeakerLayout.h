#pragma once

#include <SpatialCore/Core/Types.h>
#include <cstdint>
#include <vector>

namespace spatialcore
{

struct VirtualSpeaker
{
    float azimuthRad   = 0.0f;
    float elevationRad = 0.0f;
};

static_assert (MAX_SPEAKERS <= 16,
    "VBAPTriplet::nadirMask is a 16-bit mask (one bit per speaker) -- widen it "
    "before raising MAX_SPEAKERS above 16");

struct VBAPTriplet
{
    int i = 0, j = 0, k = 0;
    float inv[3][3] = {};

    // --- Lower-hemisphere triplets (ITU-R BS.2127 / EAR construction, D-04) ---
    // Ordinary triplets leave these at their defaults, so code that builds a
    // VBAPTriplet field by field (OpenSpatialDelay) still compiles and behaves
    // identically (DR-3).
    //
    // For a virtual -30 degree vertex the i/j/k slot holds the index of the
    // ear-level speaker it downmixes onto, so duplicate indices are legal and
    // gains accumulate. `inv` is always built from the vertex's real direction.

    /** true: only consulted when no ordinary triplet contains the source. */
    bool lowerHemisphere = false;
    /** 0..2 = which of the i/j/k slots is the virtual nadir (that slot's index
        is ignored); -1 = this triplet has no nadir vertex. */
    int nadirVertex = -1;
    /** Bit s set = ear-level speaker s receives the nadir share. */
    std::uint16_t nadirMask = 0;
    /** Per-speaker share of the nadir gain: 1 / sqrt (number of mask bits). */
    float nadirGain = 0.0f;
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

static_assert (NUM_LAYOUT_DEFS == 15,
    "LayoutID count changed -- update CLAUDE.md, README.md, "
    "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
    "spatialcore-architecture.md");

const SpeakerLayout& getLayoutDef (LayoutID id);

// v1.0: Check if a speaker layout has height speakers: any speaker with
// |elevation| > 0.0175 rad (about 1 degree). buildVBAPTripletsForLayout uses
// this same predicate, so a layout is height-and-triangulated or
// flat-and-untriangulated, never mismatched (D-03).
bool layoutHasHeight (const SpeakerLayout& layout);

// Build 3D VBAP triplets for a speaker layout with height speakers (empty
// result for 2D-only layouts -- VBAP uses pair-wise panning, no triplets needed)
void buildVBAPTripletsForLayout (const SpeakerLayout& layout,
                                  std::vector<VBAPTriplet>& triplets);

// Append the ITU-R BS.2127 (EAR) lower-hemisphere triplets (D-04): the hull of
// the ear-level speakers, a virtual -30 degree copy under each, and a virtual
// nadir. Each appended triplet has lowerHemisphere == true.
//
// Appends and never clears. Message/prepare thread only (allocates via
// push_back) -- never call from processBlock. Appends nothing for flat layouts,
// for layouts with any speaker below -10 degrees, or with fewer than 3
// ear-level speakers.
//
// Deliberately separate from buildVBAPTripletsForLayout: the regular list must
// stay regular-only so the RenderEngine::activateLayout guard and the
// [io][layout] test keep seeing it alone.
void appendLowerHemisphereTriplets (const SpeakerLayout& layout,
                                    std::vector<VBAPTriplet>& triplets);

} // namespace spatialcore
