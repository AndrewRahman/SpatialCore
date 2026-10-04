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

    // Kinds (WR-02). A triplet list holds three kinds of entry, and
    // computeVBAPGains3D tries them in this order, each only when every earlier
    // kind enclosed nothing:
    //
    //   kind()     lowerHemisphere  nadirVertex  nadirMask  nadirGain
    //   regular    false            (ignored)    (ignored)  (ignored)
    //   nadirCap   true             0..2         != 0       1/sqrt(mask bits)
    //   pairWedge  true             0..2         == 0       0
    //
    // A nadir cap is two virtual -30 degree copies of a neighbouring ear-level
    // pair plus the virtual nadir, whose share is spread over nadirMask.
    // A pair-pan wedge (G-02-2) is that pair's two ear-level speakers plus the
    // virtual nadir, whose share is discarded, so the result is the pair's 2D
    // pan at the source azimuth at every elevation down to the nadir cap (EAR's
    // QuadRegion result). nadirMask == 0 is what makes a lower-hemisphere
    // triplet a wedge: a cap must always have at least one mask bit. kind() is
    // the one classifier; the engine and the tests all call it.

    enum class Kind : std::uint8_t { regular = 0, nadirCap = 1, pairWedge = 2 };

    /** Derived from the fields below, never stored, so a triplet built field
        by field (DR-3) cannot disagree with it. Integer tests only. */
    Kind kind() const noexcept
    {
        if (! lowerHemisphere)
            return Kind::regular;
        if (nadirVertex >= 0 && nadirMask == 0)
            return Kind::pairWedge;
        return Kind::nadirCap;
    }

    /** true: a lower-hemisphere triplet, a nadir cap or a pair-pan wedge (see
        kind()); both are consulted only when no regular triplet contains the
        source, caps before wedges. */
    bool lowerHemisphere = false;
    /** 0..2 = which of the i/j/k slots is the virtual nadir (that slot's index
        is ignored); -1 = this triplet has no nadir vertex. */
    int nadirVertex = -1;
    /** Bit s set = ear-level speaker s receives the nadir share.
     *  0 on a pair-pan wedge, and only there. */
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
// Ear-level means within 10 degrees of the horizon, and the construction uses
// each ear-level speaker's real elevation (the virtual copies sit at an
// absolute -30 degrees). A wedge's result equals its pair's horizon pan, the
// EAR QuadRegion result the docs describe, exactly when the pair's two
// speakers have equal |elevation| (every shipped layout has them all at 0).
// When they differ, each gain is divided by the cosine of its speaker's
// elevation before renormalising: a small departure from that construction
// (at most 0.006 in gain for a 0/10-degree pair 30 degrees apart) (IN-05).
//
// Appends and never clears. Message/prepare thread only (allocates via
// push_back) -- never call from processBlock. Appends nothing for flat layouts,
// for layouts with any speaker below -10 degrees, or with fewer than 3
// ear-level speakers.
//
// Deliberately separate from buildVBAPTripletsForLayout: the regular list must
// stay regular-only so the RenderEngine::activateLayout guard and the
// [io][layout] test keep seeing it alone. A caller of computeVBAPGains3D must
// pass the result of both builders, in that order: the regular list alone does
// not enclose any below-horizon direction (see computeVBAPGains3D, WR-03).
void appendLowerHemisphereTriplets (const SpeakerLayout& layout,
                                    std::vector<VBAPTriplet>& triplets);

} // namespace spatialcore
