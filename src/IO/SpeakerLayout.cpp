#include <SpatialCore/IO/SpeakerLayout.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// Helper: degrees to radians
//==============================================================================
static constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

static SpeakerLayout makeLayout(int numSpk, int lfeIdx, int totalCh,
                                 const float azimuths[], const float elevations[],
                                 const int channels[])
{
    SpeakerLayout l = {};
    l.numSpeakers     = numSpk;
    l.lfeChannelIndex = lfeIdx;
    l.totalChannels   = totalCh;
    for (int i = 0; i < numSpk; ++i)
    {
        l.speakers[i].azimuthRad   = azimuths[i] * kDegToRad;
        l.speakers[i].elevationRad = elevations[i] * kDegToRad;
        l.speakers[i].channelIndex = channels[i];
    }
    return l;
}

namespace Layouts
{

SpeakerLayout getQuad()
{
    const float az[] = { 45, -45, 135, -135 };
    const float el[] = { 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 3 };
    return makeLayout(4, -1, 4, az, el, ch);
}

SpeakerLayout get5_0()
{
    const float az[] = { 30, -30, 0, 110, -110 };
    const float el[] = { 0, 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 3, 4 };
    return makeLayout(5, -1, 5, az, el, ch);
}

SpeakerLayout get5_1()
{
    const float az[] = { 30, -30, 0, 110, -110 };
    const float el[] = { 0, 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 4, 5 };
    return makeLayout(5, 3, 6, az, el, ch);
}

SpeakerLayout get7_0()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 3, 4, 5, 6 };
    return makeLayout(7, -1, 7, az, el, ch);
}

SpeakerLayout get7_1()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7 };
    return makeLayout(7, 3, 8, az, el, ch);
}

SpeakerLayout get9_1()
{
    // ITU-R BS.2051 System H: 9 ear-level speakers + LFE, no height
    const float az[] = { 30, -30, 0, 90, -90, 135, -135, 60, -60 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9 };
    return makeLayout(9, 3, 10, az, el, ch);
}

SpeakerLayout getOctaphonic()
{
    // "Center" configuration: 45-degree intervals starting at front
    const float az[] = { 0, -45, -90, -135, 180, 135, 90, 45 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const int   ch[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    return makeLayout(8, -1, 8, az, el, ch);
}

SpeakerLayout get5_1_2()
{
    const float az[] = { 30, -30, 0, 110, -110, 90, -90 };
    const float el[] = { 0, 0, 0, 0, 0, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7 };
    return makeLayout(7, 3, 8, az, el, ch);
}

SpeakerLayout get5_1_4()
{
    const float az[] = { 30, -30, 0, 110, -110, 45, -45, 135, -135 };
    const float el[] = { 0, 0, 0, 0, 0, 45, 45, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9 };
    return makeLayout(9, 3, 10, az, el, ch);
}

SpeakerLayout get7_1_2()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135, 90, -90 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9 };
    return makeLayout(9, 3, 10, az, el, ch);
}

SpeakerLayout get7_1_4()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135, 45, -45, 135, -135 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 45, 45, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11 };
    return makeLayout(11, 3, 12, az, el, ch);
}

SpeakerLayout get7_1_6()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135, 45, -45, 135, -135, 90, -90 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 45, 45, 45, 45, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
    return makeLayout(13, 3, 14, az, el, ch);
}

SpeakerLayout get9_1_4()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135, 60, -60, 45, -45, 135, -135 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 45, 45, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
    return makeLayout(13, 3, 14, az, el, ch);
}

SpeakerLayout get9_1_6()
{
    const float az[] = { 30, -30, 0, 90, -90, 135, -135, 60, -60, 45, -45, 90, -90, 135, -135 };
    const float el[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 45, 45, 45, 45, 45, 45 };
    const int   ch[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    return makeLayout(15, 3, 16, az, el, ch);
}

SpeakerLayout getSML13_1()
{
    // Spatial Media Lab Multi-Use Room (13 speakers + LFE)
    // Ear level (8 at El=0), Height (4 at El=45), Zenith (1 at El=90)
    // IEM AllRADecoder config -- positive azimuth = left
    const float az[] = { 0, -45, -90, -135, 180, 135, 90, 45, -45, -135, 135, 45, 0 };
    const float el[] = { 0,   0,   0,    0,   0,   0,  0,  0,  45,   45,  45, 45, 90 };
    const int   ch[] = { 0,   1,   2,    3,   4,   5,  6,  7,   8,    9,  10, 11, 12 };
    return makeLayout(13, 13, 14, az, el, ch);
}

SpeakerLayout getVirtualBinaural16()
{
    // 16 virtual speakers evenly spaced for binaural rendering
    SpeakerLayout l = {};
    l.numSpeakers     = 16;
    l.lfeChannelIndex = -1;
    l.totalChannels   = 16;
    for (int i = 0; i < 16; ++i)
    {
        float az = 360.0f * static_cast<float>(i) / 16.0f;
        if (az > 180.0f) az -= 360.0f;
        l.speakers[i].azimuthRad   = az * kDegToRad;
        l.speakers[i].elevationRad = 0.0f;
        l.speakers[i].channelIndex = i;
    }
    return l;
}

} // namespace Layouts

//==============================================================================
// Build 3D VBAP triplets for a speaker layout with height speakers
//==============================================================================
void buildVBAPTripletsForLayout(const SpeakerLayout& layout,
                                std::vector<VBAPTriplet>& triplets)
{
    triplets.clear();
    const int N = layout.numSpeakers;

    // Check if layout has height speakers (any elevation != 0)
    bool hasHeight = false;
    for (int s = 0; s < N; ++s)
        if (std::abs(layout.speakers[s].elevationRad) > 0.01f)
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

                auto ca  = toCart(layout.speakers[a].azimuthRad,  layout.speakers[a].elevationRad);
                auto cb  = toCart(layout.speakers[b].azimuthRad,  layout.speakers[b].elevationRad);
                auto ccc = toCart(layout.speakers[cc].azimuthRad, layout.speakers[cc].elevationRad);

                float m[3][3] = {
                    { ca[0], cb[0], ccc[0] },
                    { ca[1], cb[1], ccc[1] },
                    { ca[2], cb[2], ccc[2] }
                };

                float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                          - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                          + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

                if (std::abs(det) < 0.01f)
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

                triplets.push_back(t);
            }
        }
    }
}

} // namespace spatialcore
