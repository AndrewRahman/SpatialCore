#include <SpatialCore/IO/SpeakerLayout.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace spatialcore
{

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float degToRad (float deg) { return deg * kPi / 180.0f; }

//==============================================================================
// Table-driven speaker layout definitions, moved verbatim from
// Source/PluginProcessor.cpp (layoutDefs / makeLayoutFromDef / LayoutID)
//==============================================================================
struct SpeakerDef { float azDeg; float elDeg; int chIdx; };
struct LayoutDef { int numSpeakers; int lfeIdx; int totalChs; SpeakerDef speakers[16]; };

static const LayoutDef layoutDefs[NUM_LAYOUT_DEFS] = {
    // Quad (4.0) -- symmetric 90 deg spacing
    { 4, -1, 4, {{ 45,0,0}, {-45,0,1}, { 135,0,2}, {-135,0,3}} },
    // 5.0
    { 5, -1, 5, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 110,0,3}, {-110,0,4}} },
    // 5.1 (LFE=ch3)
    { 5,  3, 6, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 110,0,4}, {-110,0,5}} },
    // 7.0
    { 7, -1, 7, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,3}, {-90,0,4}, { 135,0,5}, {-135,0,6}} },
    // 7.1 (LFE=ch3)
    { 7,  3, 8, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}} },
    // 9.1 (LFE=ch3) -- ITU-R BS.2051 System H, 9 ear-level speakers + LFE, no height
    { 9, 3, 10, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}, { 60,0,8}, {-60,0,9}} },
    // 5.1.2 (LFE=ch3)
    { 7,  3, 8, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 110,0,4}, {-110,0,5}, { 90,45,6}, {-90,45,7}} },
    // 5.1.4 (LFE=ch3)
    { 9,  3, 10, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 110,0,4}, {-110,0,5}, { 45,45,6}, {-45,45,7}, { 135,45,8}, {-135,45,9}} },
    // 7.1.2 (LFE=ch3)
    { 9,  3, 10, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}, { 90,45,8}, {-90,45,9}} },
    // 7.1.4 (LFE=ch3)
    { 11, 3, 12, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}, { 45,45,8}, {-45,45,9}, { 135,45,10}, {-135,45,11}} },
    // 7.1.6 (LFE=ch3)
    { 13, 3, 14, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}, { 45,45,8}, {-45,45,9}, { 135,45,10}, {-135,45,11}, { 90,45,12}, {-90,45,13}} },
    // 9.1.4 (LFE=ch3)
    { 13, 3, 14, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}, { 60,0,8}, {-60,0,9}, { 45,45,10}, {-45,45,11}, { 135,45,12}, {-135,45,13}} },
    // 9.1.6 (LFE=ch3)
    { 15, 3, 16, {{ 30,0,0}, {-30,0,1}, {0,0,2}, { 90,0,4}, {-90,0,5}, { 135,0,6}, {-135,0,7}, { 60,0,8}, {-60,0,9}, { 45,45,10}, {-45,45,11}, { 90,45,12}, {-90,45,13}, { 135,45,14}, {-135,45,15}} },
    // Octaphonic (8.0, no LFE) -- "Center" configuration, 45 deg intervals
    { 8, -1, 8, {{0,0,0}, {-45,0,1}, {-90,0,2}, {-135,0,3}, { 180,0,4}, { 135,0,5}, { 90,0,6}, { 45,0,7}} },
    // SML 13.1 -- Spatial Media Lab Multi-Use Room (13 speakers + LFE=ch13)
    // Ear level (8 at El=0 deg), Height (4 at El=45 deg), Zenith (1 at El=90 deg)
    // IEM AllRADecoder config -- positive azimuth = left (matches OSD convention)
    { 13, 13, 14, {
        {   0,  0, 0},  // FC   -- Front Center
        { -45,  0, 1},  // FR   -- Front Right
        { -90,  0, 2},  // R    -- Right
        {-135,  0, 3},  // RR   -- Rear Right
        { 180,  0, 4},  // RC   -- Rear Center
        { 135,  0, 5},  // RL   -- Rear Left
        {  90,  0, 6},  // L    -- Left
        {  45,  0, 7},  // FL   -- Front Left
        { -45, 45, 8},  // UFR  -- Upper Front Right
        {-135, 45, 9},  // URR  -- Upper Rear Right
        { 135, 45,10},  // URL  -- Upper Rear Left
        {  45, 45,11},  // UFL  -- Upper Front Left
        {   0, 90,12},  // T    -- Top (Zenith)
    }},
};

static SpeakerLayout makeLayoutFromDef (const LayoutDef& def)
{
    SpeakerLayout l = {};
    l.numSpeakers = def.numSpeakers;
    l.lfeChannelIndex = def.lfeIdx;
    l.totalChannels = def.totalChs;
    for (int i = 0; i < def.numSpeakers; ++i)
        l.speakers[i] = { degToRad (def.speakers[i].azDeg), degToRad (def.speakers[i].elDeg), def.speakers[i].chIdx };
    return l;
}

const SpeakerLayout& getLayoutDef (LayoutID id)
{
    static const std::array<SpeakerLayout, NUM_LAYOUT_DEFS> cached = []() {
        std::array<SpeakerLayout, NUM_LAYOUT_DEFS> result;
        for (int i = 0; i < NUM_LAYOUT_DEFS; ++i)
            result[static_cast<size_t> (i)] = makeLayoutFromDef (layoutDefs[i]);
        return result;
    }();
    return cached[static_cast<size_t> (id)];
}

namespace
{
// The one height threshold (D-03): about 1 degree. layoutHasHeight and
// buildVBAPTripletsForLayout both use it, so a layout is either height-and-
// triangulated or flat-and-untriangulated, never mismatched.
constexpr float kHeightThresholdRad = 0.0175f;
} // namespace

// v1.0: Check if a speaker layout has height speakers (|elevation| > 0.0175 rad)
bool layoutHasHeight (const SpeakerLayout& layout)
{
    for (int s = 0; s < layout.numSpeakers; ++s)
        if (std::abs (layout.speakers[s].elevationRad) > kHeightThresholdRad)
            return true;
    return false;
}

//==============================================================================
// Build 3D VBAP triplets for a speaker layout with height speakers
//==============================================================================
void buildVBAPTripletsForLayout (const SpeakerLayout& layout,
                                  std::vector<VBAPTriplet>& triplets)
{
    triplets.clear();
    const int N = layout.numSpeakers;

    // One shared height predicate with the public height test (D-03).
    if (! layoutHasHeight (layout))
        return;  // 2D-only layout -- VBAP uses pair-wise panning, no triplets needed

    for (int a = 0; a < N - 2; ++a)
    {
        for (int b = a + 1; b < N - 1; ++b)
        {
            for (int cc = b + 1; cc < N; ++cc)
            {
                auto toCart = [](float az, float el) -> std::array<float, 3> {
                    return { std::cos(el) * std::sin(az),
                             std::cos(el) * std::cos(az),
                             std::sin(el) };
                };

                auto ca  = toCart (layout.speakers[a].azimuthRad,  layout.speakers[a].elevationRad);
                auto cb  = toCart (layout.speakers[b].azimuthRad,  layout.speakers[b].elevationRad);
                auto ccc = toCart (layout.speakers[cc].azimuthRad, layout.speakers[cc].elevationRad);

                float m[3][3] = {
                    { ca[0], cb[0], ccc[0] },
                    { ca[1], cb[1], ccc[1] },
                    { ca[2], cb[2], ccc[2] }
                };

                float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                          - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                          + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

                if (std::abs (det) < 0.01f)
                    continue;

                float invDet = 1.0f / det;
                VBAPTriplet t;
                t.i = a;
                t.j = b;
                t.k = cc;
                t.inv[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
                t.inv[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet;
                t.inv[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
                t.inv[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet;
                t.inv[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
                t.inv[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet;
                t.inv[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
                t.inv[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet;
                t.inv[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

                triplets.push_back (t);
            }
        }
    }
}

//==============================================================================
// Lower-hemisphere triplets (D-04, ITU-R BS.2127 / EAR construction).
//
// Built once at layout-build time on the message/prepare thread (it allocates);
// computeVBAPGains3D only ever reads the result through a const reference.
//
// Hull of {ear-level speakers, a virtual -30 degree copy under each, a virtual
// nadir}. The lower-hemisphere-only hull is deliberate (RESEARCH F4): a full-
// sphere hull has facets that skip the ear-level speakers on sparse-rear
// layouts and pan below-horizon sources onto height speakers.
//
// Output, for n ear-level speakers, is 2n triplets (G-02-2):
//   - n nadir-cap triangles: the two -30 degree copies of a neighbouring pair
//     plus the virtual nadir (the hull's own facets that touch the nadir,
//     matching EAR's VirtualNgon exactly);
//   - n pair-pan wedges: the same neighbouring pair's two real ear-level
//     speakers plus the virtual nadir, with a zero nadir share, so the result
//     is the pair's 2D horizon pan at the source azimuth.
// The hull's remaining facets are the ear-level/-30 degree trapezoids. A
// planar trapezoid's triangulations all have the same gain sum, so the
// minimum-sum rule cannot choose between the overlapping triangles (it
// breaks exact and 1-2 ULP ties by enumeration order, leaning the pan to
// either side), and no triangulation reproduces EAR. EAR pans each trapezoid
// as one QuadRegion; with the -30 copies downmixed 1:1 onto their ear-level
// speakers that collapses to the pair's horizon pan, which is what the wedge
// computes. The equivalence assumes the ear-level speakers sit at 0 degrees
// elevation, as every shipped layout does.
//
// Gap bridge (WR-05): the construction above needs the ear-level ring to
// surround the listener. When two neighbouring ear-level speakers A and B are
// kGapBridgeRad (179 degrees) or more apart in azimuth, the hull no longer
// contains the origin, its "away from the origin" facet orientation fails and
// the wrapping pair's cap and wedge are lost, leaving that part of the lower
// hemisphere unenclosed. Each such gap gets two virtual ear-level vertices, at
// one third and two thirds of the way from A to B, downmixed 1:1 onto A and B
// respectively (exactly as a -30 degree copy downmixes onto its speaker). The
// ring then surrounds the listener again and gets its caps and wedges as
// usual: the first third of the gap stays on A, the middle third pans A to B,
// the last third stays on B. No shipped layout has such a gap (the widest is
// 140 degrees), so their triplets are unchanged bit for bit.
//==============================================================================
namespace
{
constexpr double kPiD                 = 3.14159265358979323846;
constexpr double kEarLevelLimitRad    = 10.0 * kPiD / 180.0;
constexpr double kVirtualRingElevRad  = -30.0 * kPiD / 180.0;
constexpr double kHullDetEpsilon      = 1e-3;
constexpr double kSupportPlaneEpsilon = 1e-7;
// 1 degree short of 180: from about 179.93 degrees the wrapping cap's
// determinant (0.75 sin gap) already falls under kHullDetEpsilon.
constexpr double kGapBridgeRad        = 179.0 * kPiD / 180.0;

struct HullVertex
{
    double x, y, z;
    int    target;      // real speaker index this vertex downmixes onto
    bool   isVirtual;
    bool   isNadir;
    double azRad;       // azimuth the vertex was built from (copies reuse it)
};

HullVertex makeHullVertex (double azRad, double elRad, int target, bool isVirtual)
{
    return { std::cos (elRad) * std::sin (azRad),
             std::cos (elRad) * std::cos (azRad),
             std::sin (elRad),
             target, isVirtual, false, azRad };
}

// Row r of t.inv times a direction p is the gain of slot r. m holds the three
// vertex directions as columns; invDet is 1 / det (m).
void setInverse (VBAPTriplet& t, const double m[3][3], double invDet)
{
    t.inv[0][0] = static_cast<float> (  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet);
    t.inv[0][1] = static_cast<float> ( -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet);
    t.inv[0][2] = static_cast<float> (  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet);
    t.inv[1][0] = static_cast<float> ( -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet);
    t.inv[1][1] = static_cast<float> (  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet);
    t.inv[1][2] = static_cast<float> ( -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet);
    t.inv[2][0] = static_cast<float> (  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet);
    t.inv[2][1] = static_cast<float> ( -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet);
    t.inv[2][2] = static_cast<float> (  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet);
}
} // namespace

void appendLowerHemisphereTriplets (const SpeakerLayout& layout,
                                    std::vector<VBAPTriplet>& triplets)
{
    if (! layoutHasHeight (layout))
        return;

    const int N = layout.numSpeakers;

    // A speaker below the ear-level band would cover the lower hemisphere
    // natively; no shipped layout has one, so treat it as "no extras".
    for (int s = 0; s < N; ++s)
        if (static_cast<double> (layout.speakers[s].elevationRad) < -kEarLevelLimitRad)
            return;

    std::vector<HullVertex> verts;
    verts.reserve (static_cast<size_t> (2 * (N + 4) + 1));

    std::uint16_t earMask = 0;
    int earCount = 0;
    int firstEar = -1;

    for (int s = 0; s < N; ++s)
    {
        const double az = static_cast<double> (layout.speakers[s].azimuthRad);
        const double el = static_cast<double> (layout.speakers[s].elevationRad);
        if (std::abs (el) > kEarLevelLimitRad)
            continue;

        verts.push_back (makeHullVertex (az, el, s, false));
        earMask = static_cast<std::uint16_t> (earMask | (1u << s));
        ++earCount;
        if (firstEar < 0)
            firstEar = s;
    }

    if (earCount < 3)
        return;

    // Gap bridge (WR-05, see above): two virtual ear-level vertices in every
    // neighbouring-pair azimuth gap of kGapBridgeRad or more.
    {
        std::vector<std::pair<double, size_t>> ring;   // (azimuth in [0, 2 pi), vertex)
        ring.reserve (verts.size());
        for (size_t v = 0; v < verts.size(); ++v)
        {
            double a = std::atan2 (verts[v].x, verts[v].y);
            if (a < 0.0)
                a += 2.0 * kPiD;
            ring.emplace_back (a, v);
        }
        std::sort (ring.begin(), ring.end());

        const size_t m = ring.size();
        for (size_t p = 0; p < m; ++p)
        {
            const size_t q = (p + 1) % m;
            double gap = ring[q].first - ring[p].first;
            if (q == 0)
                gap += 2.0 * kPiD;
            if (gap < kGapBridgeRad)
                continue;

            const int targetA = verts[ring[p].second].target;
            const int targetB = verts[ring[q].second].target;
            verts.push_back (makeHullVertex (ring[p].first + gap / 3.0,       0.0, targetA, true));
            verts.push_back (makeHullVertex (ring[p].first + 2.0 * gap / 3.0, 0.0, targetB, true));
        }
    }

    // Ear-level ring = the real ear-level speakers, then any bridge vertices.
    const size_t numEar = verts.size();
    for (size_t v = 0; v < numEar; ++v)
        verts.push_back (makeHullVertex (verts[v].azRad, kVirtualRingElevRad, verts[v].target, true));

    HullVertex nadir { 0.0, 0.0, -1.0, firstEar, true, true, 0.0 };
    verts.push_back (nadir);

    const float nadirShare = static_cast<float> (1.0 / std::sqrt (static_cast<double> (earCount)));
    const size_t V = verts.size();

    // Ear-level ring vertex a hull vertex belongs to (a -30 degree copy maps to
    // the ear-level vertex it was made from).
    auto earVertexOf = [numEar] (size_t v) { return v < numEar ? v : v - numEar; };

    std::vector<std::pair<size_t, size_t>> capPairs;
    capPairs.reserve (numEar);

    for (size_t a = 0; a + 2 < V; ++a)
    {
        for (size_t b = a + 1; b + 1 < V; ++b)
        {
            for (size_t c = b + 1; c < V; ++c)
            {
                const HullVertex& va = verts[a];
                const HullVertex& vb = verts[b];
                const HullVertex& vc = verts[c];

                // Regular (all real) triples belong to buildVBAPTripletsForLayout.
                if (! (va.isVirtual || vb.isVirtual || vc.isVirtual))
                    continue;

                const double m[3][3] = {
                    { va.x, vb.x, vc.x },
                    { va.y, vb.y, vc.y },
                    { va.z, vb.z, vc.z }
                };

                const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                                 - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                                 + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

                if (std::abs (det) < kHullDetEpsilon)
                    continue;

                // Plane normal (b - a) x (c - a), oriented away from the origin.
                // Correct because the (bridged) ear-level ring surrounds it.
                const double e1x = vb.x - va.x, e1y = vb.y - va.y, e1z = vb.z - va.z;
                const double e2x = vc.x - va.x, e2y = vc.y - va.y, e2z = vc.z - va.z;
                double nx = e1y * e2z - e1z * e2y;
                double ny = e1z * e2x - e1x * e2z;
                double nz = e1x * e2y - e1y * e2x;
                if (nx * va.x + ny * va.y + nz * va.z < 0.0)
                {
                    nx = -nx;  ny = -ny;  nz = -nz;
                }

                // Keep only supporting planes (hull facets): no vertex may lie
                // outside.
                bool isFacet = true;
                for (size_t v = 0; v < V; ++v)
                {
                    const double side = nx * (verts[v].x - va.x)
                                      + ny * (verts[v].y - va.y)
                                      + nz * (verts[v].z - va.z);
                    if (side > kSupportPlaneEpsilon)
                    {
                        isFacet = false;
                        break;
                    }
                }
                if (! isFacet)
                    continue;

                // The only facets without the nadir are the four triangles of
                // each ear-level/-30 degree trapezoid. They are not emitted:
                // the pair-pan wedges below replace them (G-02-2, D-04).
                if (! (va.isNadir || vb.isNadir || vc.isNadir))
                    continue;

                const double invDet = 1.0 / det;
                VBAPTriplet t;
                t.i = va.target;
                t.j = vb.target;
                t.k = vc.target;
                setInverse (t, m, invDet);

                t.lowerHemisphere = true;
                t.nadirVertex = va.isNadir ? 0 : (vb.isNadir ? 1 : 2);
                t.nadirMask = earMask;
                t.nadirGain = nadirShare;

                triplets.push_back (t);

                // Remember the neighbouring ear-level pair this cap spans (slot
                // order, nadir slot skipped) so its wedge can be built below.
                const size_t pairA = (t.nadirVertex == 0) ? b : a;
                const size_t pairB = (t.nadirVertex == 2) ? b : c;
                capPairs.emplace_back (earVertexOf (pairA), earVertexOf (pairB));
            }
        }
    }

    // One pair-pan wedge per neighbouring ear-level pair: the pair's two
    // ear-level vertices (real speakers, or a bridge vertex in a WR-05 gap)
    // plus the virtual nadir, nadir share 0. Slot 2 is the nadir; k is a
    // placeholder that nadirVertex makes the output mapping ignore.
    for (const auto& pair : capPairs)
    {
        // Ear-level ring vertices: a real speaker's is built from its own
        // azimuth and elevation; a bridge vertex downmixes onto its target.
        const HullVertex& va = verts[pair.first];
        const HullVertex& vb = verts[pair.second];

        const double m[3][3] = {
            { va.x, vb.x, nadir.x },
            { va.y, vb.y, nadir.y },
            { va.z, vb.z, nadir.z }
        };

        const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

        if (std::abs (det) < kHullDetEpsilon)
            continue;

        VBAPTriplet w;
        w.i = va.target;
        w.j = vb.target;
        w.k = firstEar;
        setInverse (w, m, 1.0 / det);

        w.lowerHemisphere = true;
        w.nadirVertex = 2;
        w.nadirMask = 0;
        w.nadirGain = 0.0f;

        triplets.push_back (w);
    }
}

} // namespace spatialcore
