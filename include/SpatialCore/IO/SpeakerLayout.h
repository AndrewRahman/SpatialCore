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

    // A pair-pan wedge (G-02-2) is an ordinary lower-hemisphere triplet whose
    // nadir slot carries no share: nadirVertex >= 0 with nadirMask == 0 and
    // nadirGain == 0. It is two neighbouring ear-level speakers plus the virtual
    // nadir, whose share is discarded, so the result is that pair's 2D pan at
    // the source azimuth at every elevation down to the nadir cap (EAR's
    // QuadRegion result). It is tried only after every regular triplet and every
    // nadir-cap triangle.

    /** true: only consulted when no ordinary triplet contains the source. */
    bool lowerHemisphere = false;
    /** 0..2 = which of the i/j/k slots is the virtual nadir (that slot's index
        is ignored); -1 = this triplet has no nadir vertex. */
    int nadirVertex = -1;
    /** Bit s set = ear-level speaker s receives the nadir share.
     *  0 on a pair-pan wedge. */
    std::uint16_t nadirMask = 0;
    /** Per-speaker share of the nadir gain: 1 / sqrt (number of mask bits).
     *  0 on a pair-pan wedge. */
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

// Append the ITU-R BS.2127 (EAR) lower-hemisphere triplets (D-04). Per
// neighbouring ear-level pair it appends one nadir-cap triangle (the pair's two
// virtual -30 degree copies plus the virtual nadir) and one pair-pan wedge (the
// pair's two ear-level speakers plus the nadir, zero nadir share), so 2n
// triplets for n ear-level speakers (G-02-2). Each appended triplet has
// lowerHemisphere == true.
//
// Ear-level gaps (WR-05): when two neighbouring ear-level speakers are 179
// degrees or more apart in azimuth (the ring does not surround the listener),
// that gap gets two virtual ear-level vertices at one third and two thirds of
// the way across, downmixed onto the nearer of the two speakers. The first
// third of the gap stays on one speaker, the middle third pans between the two,
// the last third stays on the other, so every below-horizon direction is
// enclosed and audible. The count is then 2 (n + 2g) for g such gaps (g is 0 on
// every shipped layout, so they get exactly 2n).
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
