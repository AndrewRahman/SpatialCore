#include <SpatialCore/IO/SpeakerLayout.h>
#include <algorithm>
#include <array>
#include <cmath>

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

// v1.0: Check if a speaker layout has height speakers (elevation > 1 degree)
bool layoutHasHeight (const SpeakerLayout& layout)
{
    for (int s = 0; s < layout.numSpeakers; ++s)
        if (std::abs (layout.speakers[s].elevationRad) > 0.0175f)  // ~1 degree
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

    // Check if layout has height speakers (any elevation != 0)
    bool hasHeight = false;
    for (int s = 0; s < N; ++s)
        if (std::abs (layout.speakers[s].elevationRad) > 0.01f)
        { hasHeight = true; break; }

    if (! hasHeight)
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

} // namespace spatialcore
