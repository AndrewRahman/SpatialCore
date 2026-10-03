#include <SpatialCore/Core/SpatialMath.h>
#include "FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU

#include <algorithm>
#include <cmath>
#include <limits>

namespace spatialcore
{

//==============================================================================
// Modular 3D Audio Core -- 6th-Order Ambisonics (49 channels)
// Real SH, ACN channel order, SN3D normalisation, no Condon-Shortley phase,
// radians, az 0 = front, +az toward +Y (left), el 0 = horizon, +el up (AmbiX).
// The single SH implementation (D-08); full convention statement on the
// declaration -- see SpatialMath.h.
//==============================================================================

float evalSH (int acn, float az, float el)
{
    // Real spherical harmonics, ACN ordering, SN3D normalization
    // az = azimuth (radians), el = elevation (radians)
    float cosAz  = std::cos (az);
    float sinAz  = std::sin (az);
    float cos2Az = std::cos (2.0f * az);
    float sin2Az = std::sin (2.0f * az);
    float cos3Az = std::cos (3.0f * az);
    float sin3Az = std::sin (3.0f * az);
    float sinEl  = std::sin (el);
    float cosEl  = std::cos (el);
    float sinEl2 = sinEl * sinEl;
    float cosEl2 = cosEl * cosEl;

    // Higher-order trig (computed only when needed via fallthrough to default check)
    float cos4Az = 0.0f, sin4Az = 0.0f, cos5Az = 0.0f, sin5Az = 0.0f, cos6Az = 0.0f, sin6Az = 0.0f;
    float cosEl3 = 0.0f, cosEl4 = 0.0f, cosEl5 = 0.0f, cosEl6 = 0.0f;
    float sinEl4 = 0.0f;

    if (acn >= 16)
    {
        cos4Az = std::cos (4.0f * az);  sin4Az = std::sin (4.0f * az);
        cosEl3 = cosEl2 * cosEl;        cosEl4 = cosEl2 * cosEl2;
        sinEl4 = sinEl2 * sinEl2;
        if (acn >= 25)
        {
            cos5Az = std::cos (5.0f * az);  sin5Az = std::sin (5.0f * az);
            cosEl5 = cosEl4 * cosEl;
        }
        if (acn >= 36)
        {
            cos6Az = std::cos (6.0f * az);  sin6Az = std::sin (6.0f * az);
            cosEl6 = cosEl4 * cosEl2;
        }
    }

    switch (acn)
    {
        // Order 0
        case 0: return 1.0f;

        // Order 1 (SN3D: no extra factor needed)
        case 1: return sinAz * cosEl;              // Y1^-1
        case 2: return sinEl;                       // Y1^0
        case 3: return cosAz * cosEl;              // Y1^1

        // Order 2 (SN3D normalization)
        case 4: return std::sqrt (3.0f) * 0.5f * sin2Az * cosEl2;                     // Y2^-2
        case 5: return std::sqrt (3.0f) * sinAz * sinEl * cosEl;                      // Y2^-1
        case 6: return 0.5f * (3.0f * sinEl2 - 1.0f);                                 // Y2^0
        case 7: return std::sqrt (3.0f) * cosAz * sinEl * cosEl;                      // Y2^1
        case 8: return std::sqrt (3.0f) * 0.5f * cos2Az * cosEl2;                     // Y2^2

        // Order 3 (SN3D normalization)
        case  9: return std::sqrt (5.0f / 8.0f) * sin3Az * cosEl * cosEl2;            // Y3^-3
        case 10: return std::sqrt (15.0f) * 0.5f * sin2Az * sinEl * cosEl2;           // Y3^-2
        case 11: return std::sqrt (3.0f / 8.0f) * sinAz * cosEl * (5.0f * sinEl2 - 1.0f);  // Y3^-1
        case 12: return 0.5f * sinEl * (5.0f * sinEl2 - 3.0f);                        // Y3^0
        case 13: return std::sqrt (3.0f / 8.0f) * cosAz * cosEl * (5.0f * sinEl2 - 1.0f);  // Y3^1
        case 14: return std::sqrt (15.0f) * 0.5f * cos2Az * sinEl * cosEl2;           // Y3^2
        case 15: return std::sqrt (5.0f / 8.0f) * cos3Az * cosEl * cosEl2;            // Y3^3

        // Order 4 (SN3D normalization)
        case 16: return std::sqrt (35.0f) * 0.125f * sin4Az * cosEl4;                                  // Y4^-4
        case 17: return std::sqrt (35.0f / 8.0f) * sin3Az * sinEl * cosEl3;                            // Y4^-3
        case 18: return std::sqrt (5.0f) * 0.25f * sin2Az * cosEl2 * (7.0f * sinEl2 - 1.0f);          // Y4^-2
        case 19: return std::sqrt (5.0f / 8.0f) * sinAz * sinEl * cosEl * (7.0f * sinEl2 - 3.0f);     // Y4^-1
        case 20: return 0.125f * (35.0f * sinEl4 - 30.0f * sinEl2 + 3.0f);                             // Y4^0
        case 21: return std::sqrt (5.0f / 8.0f) * cosAz * sinEl * cosEl * (7.0f * sinEl2 - 3.0f);     // Y4^1
        case 22: return std::sqrt (5.0f) * 0.25f * cos2Az * cosEl2 * (7.0f * sinEl2 - 1.0f);          // Y4^2
        case 23: return std::sqrt (35.0f / 8.0f) * cos3Az * sinEl * cosEl3;                            // Y4^3
        case 24: return std::sqrt (35.0f) * 0.125f * cos4Az * cosEl4;                                  // Y4^4

        // Order 5 (SN3D normalization)
        case 25: return std::sqrt (63.0f / 128.0f) * sin5Az * cosEl5;                                                      // Y5^-5
        case 26: return std::sqrt (315.0f) * 0.125f * sin4Az * sinEl * cosEl4;                                            // Y5^-4
        case 27: return std::sqrt (35.0f / 128.0f) * sin3Az * cosEl3 * (9.0f * sinEl2 - 1.0f);                            // Y5^-3
        case 28: return std::sqrt (105.0f / 16.0f) * sin2Az * sinEl * cosEl2 * (3.0f * sinEl2 - 1.0f);                    // Y5^-2
        case 29: return std::sqrt (15.0f) * 0.125f * sinAz * cosEl * (21.0f * sinEl4 - 14.0f * sinEl2 + 1.0f);           // Y5^-1
        case 30: return 0.125f * sinEl * (63.0f * sinEl4 - 70.0f * sinEl2 + 15.0f);                                       // Y5^0
        case 31: return std::sqrt (15.0f) * 0.125f * cosAz * cosEl * (21.0f * sinEl4 - 14.0f * sinEl2 + 1.0f);           // Y5^1
        case 32: return std::sqrt (105.0f / 16.0f) * cos2Az * sinEl * cosEl2 * (3.0f * sinEl2 - 1.0f);                    // Y5^2
        case 33: return std::sqrt (35.0f / 128.0f) * cos3Az * cosEl3 * (9.0f * sinEl2 - 1.0f);                            // Y5^3
        case 34: return std::sqrt (315.0f) * 0.125f * cos4Az * sinEl * cosEl4;                                            // Y5^4
        case 35: return std::sqrt (63.0f / 128.0f) * cos5Az * cosEl5;                                                      // Y5^5

        // Order 6 (SN3D normalization)
        case 36: return std::sqrt (231.0f / 512.0f) * sin6Az * cosEl6;                                                    // Y6^-6
        case 37: return std::sqrt (693.0f / 128.0f) * sin5Az * sinEl * cosEl5;                                              // Y6^-5
        case 38: return std::sqrt (63.0f / 256.0f) * sin4Az * cosEl4 * (11.0f * sinEl2 - 1.0f);                           // Y6^-4
        case 39: return std::sqrt (105.0f / 128.0f) * sin3Az * sinEl * cosEl3 * (11.0f * sinEl2 - 3.0f);                  // Y6^-3
        case 40: return std::sqrt (105.0f / 32.0f) * sin2Az * cosEl2 * (33.0f * sinEl4 - 18.0f * sinEl2 + 1.0f) * 0.25f;// Y6^-2
        case 41: return std::sqrt (21.0f / 64.0f) * sinAz * sinEl * cosEl * (33.0f * sinEl4 - 30.0f * sinEl2 + 5.0f);    // Y6^-1
        case 42: return (231.0f * sinEl4 * sinEl2 - 315.0f * sinEl4 + 105.0f * sinEl2 - 5.0f) / 16.0f;                   // Y6^0
        case 43: return std::sqrt (21.0f / 64.0f) * cosAz * sinEl * cosEl * (33.0f * sinEl4 - 30.0f * sinEl2 + 5.0f);    // Y6^1
        case 44: return std::sqrt (105.0f / 32.0f) * cos2Az * cosEl2 * (33.0f * sinEl4 - 18.0f * sinEl2 + 1.0f) * 0.25f;// Y6^2
        case 45: return std::sqrt (105.0f / 128.0f) * cos3Az * sinEl * cosEl3 * (11.0f * sinEl2 - 3.0f);                  // Y6^3
        case 46: return std::sqrt (63.0f / 256.0f) * cos4Az * cosEl4 * (11.0f * sinEl2 - 1.0f);                           // Y6^4
        case 47: return std::sqrt (693.0f / 128.0f) * cos5Az * sinEl * cosEl5;                                              // Y6^5
        case 48: return std::sqrt (231.0f / 512.0f) * cos6Az * cosEl6;                                                    // Y6^6

        default: return 0.0f;
    }
}

//==============================================================================
// v0.2: 2D VBAP for flat layouts (Quad, 5.1, 7.1) -- azimuth-only panning
//==============================================================================
void computeVBAPGains2D (const SpeakerLayout& layout,
                         float azimuthRad, float* outGains)
{
    const int N = layout.numSpeakers;
    for (int s = 0; s < N; ++s)
        outGains[s] = 0.0f;

    if (N < 2) return;

    // Non-finite azimuth -> silence (D-06, D-19i). This guard must stay in a
    // .cpp: OpenSpatialDelay compiles with -ffast-math, which folds a
    // header-inline std::isfinite to true (RESEARCH F9).
    if (! std::isfinite (azimuthRad)) return;

    constexpr float kPi    = juce::MathConstants<float>::pi;
    constexpr float kTwoPi = 2.0f * juce::MathConstants<float>::pi;

    // Normalize azimuth to [-pi, pi] with one bounded std::remainder (D-19i).
    // The old unbounded while loops never terminated for |az| >~ 1e9 rad
    // (RESEARCH F7). Values already in [-pi, pi] are untouched, and up to 3*pi
    // remainder equals the old single subtraction exactly (Sterbenz), so
    // in-range and engine-produced results stay bit-identical.
    if (std::abs (azimuthRad) > kPi)
        azimuthRad = std::remainder (azimuthRad, kTwoPi);

    // Find the two speakers that span the source azimuth
    // Sort speaker azimuths for efficient pair finding
    struct SpkAz { int index; float az; };
    SpkAz sorted[MAX_SPEAKERS];
    for (int s = 0; s < N; ++s)
    {
        sorted[s].index = s;
        sorted[s].az = layout.speakers[s].azimuthRad;
        // Normalize to [-pi, pi] (bounded, D-19i)
        if (std::abs (sorted[s].az) > kPi)
            sorted[s].az = std::remainder (sorted[s].az, kTwoPi);
    }
    std::sort (sorted, sorted + N, [](const SpkAz& a, const SpkAz& b) { return a.az < b.az; });

    // Find spanning pair
    int leftIdx = -1, rightIdx = -1;
    for (int s = 0; s < N; ++s)
    {
        int next = (s + 1) % N;
        float az1 = sorted[s].az;
        float az2 = sorted[next].az;

        // Handle wrap-around
        if (next == 0)
            az2 += 2.0f * juce::MathConstants<float>::pi;

        float srcAz = azimuthRad;
        if (next == 0 && srcAz < az1)
            srcAz += 2.0f * juce::MathConstants<float>::pi;

        if (srcAz >= az1 && srcAz <= az2)
        {
            leftIdx = s;
            rightIdx = next;
            break;
        }
    }

    if (leftIdx < 0)
    {
        // Fallback: nearest speaker
        float minDist = 999.0f;
        int nearest = 0;
        for (int s = 0; s < N; ++s)
        {
            float d = std::abs (sorted[s].az - azimuthRad);
            if (d > juce::MathConstants<float>::pi) d = 2.0f * juce::MathConstants<float>::pi - d;
            if (d < minDist) { minDist = d; nearest = s; }
        }
        outGains[sorted[nearest].index] = 1.0f;
        return;
    }

    float az1 = sorted[leftIdx].az;
    float az2 = sorted[rightIdx].az;
    float srcAz = azimuthRad;

    // Handle wrap
    if (rightIdx == 0)
    {
        az2 += 2.0f * juce::MathConstants<float>::pi;
        if (srcAz < az1) srcAz += 2.0f * juce::MathConstants<float>::pi;
    }

    float span = az2 - az1;
    if (span < 1e-6f)
    {
        outGains[sorted[leftIdx].index] = 1.0f;
        return;
    }

    // Sine law panning (VBAP 2D)
    float g1 = std::sin (az2 - srcAz) / std::sin (span);
    float g2 = std::sin (srcAz - az1) / std::sin (span);

    // Constant-power normalization
    float power = g1 * g1 + g2 * g2;
    if (power > 1e-12f)
    {
        float scale = 1.0f / std::sqrt (power);
        g1 *= scale;
        g2 *= scale;
    }

    outGains[sorted[leftIdx].index] = std::max (0.0f, g1);
    outGains[sorted[rightIdx].index] = std::max (0.0f, g2);
}

//==============================================================================
// v0.2: 3D VBAP for height layouts (7.1.4, 9.1.6) -- uses pre-computed triplets
//==============================================================================
namespace
{
/** Selection tier of a triplet (see computeVBAPGains3D): 0 regular, 1 nadir
    cap, 2 pair-pan wedge, from the one classifier VBAPTriplet::kind() (WR-02). */
int selectionTier (const VBAPTriplet& t)
{
    static_assert (static_cast<int> (VBAPTriplet::Kind::regular)   == 0
                && static_cast<int> (VBAPTriplet::Kind::nadirCap)  == 1
                && static_cast<int> (VBAPTriplet::Kind::pairWedge) == 2,
                   "the pass loop in computeVBAPGains3D tries tiers 0, 1, 2 in order");
    return static_cast<int> (t.kind());
}
} // namespace

void computeVBAPGains3D (const SpeakerLayout& layout,
                         const std::vector<VBAPTriplet>& triplets,
                         float azimuthRad, float elevationRad,
                         float* outGains)
{
    const int N = layout.numSpeakers;
    for (int s = 0; s < N; ++s)
        outGains[s] = 0.0f;

    // Non-finite direction -> silence (D-06, D-19i); kept in this .cpp so a
    // -ffast-math consumer cannot fold it away (RESEARCH F9). Finite angles are
    // deliberately NOT wrapped here: there is no loop to bound, sin/cos are
    // finite for any finite input, and leaving them alone keeps every finite
    // input bit-identical to d43cb15 (D-06b).
    if (! std::isfinite (azimuthRad) || ! std::isfinite (elevationRad))
        return;

    // Source direction as unit Cartesian vector
    float px = std::cos (elevationRad) * std::sin (azimuthRad);
    float py = std::cos (elevationRad) * std::cos (azimuthRad);
    float pz = std::sin (elevationRad);

    float bestGainSum = 1e30f;   // Start high -- pick MINIMUM sum (tightest enclosing triangle)
    int bestTri = -1;
    float bestG[3] = {};

    // D-06b safety net: the triplet with the largest min (g0, g1, g2) seen by
    // the passes that ran. A later pass only runs when every earlier one found
    // nothing, so when no pass encloses the direction this candidate covers the
    // whole list.
    float fallbackMin = -std::numeric_limits<float>::infinity();
    int fallbackTri = -1;
    float fallbackG[3] = {};

    // Three tiers, each tried only when every earlier one found nothing:
    //   0  regular triplets (unchanged, so above-horizon output is bit-identical
    //      to the pre-change function: D-04, D-06b),
    //   1  nadir-cap triangles of the lower hemisphere,
    //   2  pair-pan wedges (G-02-2): one per neighbouring ear-level pair, whose
    //      result is that pair's horizon pan. Exactly one wedge encloses a band
    //      direction and no regular triplet or cap does, so the minimum-sum rule
    //      never has a tie to break below the horizon.
    for (int pass = 0; pass < 3 && bestTri < 0; ++pass)
    {
        for (int t = 0; t < static_cast<int> (triplets.size()); ++t)
        {
            const auto& tri = triplets[static_cast<size_t> (t)];
            if (selectionTier (tri) != pass)
                continue;

            float g0 = tri.inv[0][0] * px + tri.inv[0][1] * py + tri.inv[0][2] * pz;
            float g1 = tri.inv[1][0] * px + tri.inv[1][1] * py + tri.inv[1][2] * pz;
            float g2 = tri.inv[2][0] * px + tri.inv[2][1] * py + tri.inv[2][2] * pz;

            if (g0 >= -1e-6f && g1 >= -1e-6f && g2 >= -1e-6f)
            {
                float sum = g0 + g1 + g2;
                if (sum < bestGainSum)   // Min sum = tightest triangle (fixes L/R swap)
                {
                    bestGainSum = sum;
                    bestTri = t;
                    bestG[0] = std::max (0.0f, g0);
                    bestG[1] = std::max (0.0f, g1);
                    bestG[2] = std::max (0.0f, g2);
                }
            }

            const float minGain = std::min (g0, std::min (g1, g2));
            if (minGain > fallbackMin)
            {
                fallbackMin = minGain;
                fallbackTri = t;
                fallbackG[0] = g0;
                fallbackG[1] = g1;
                fallbackG[2] = g2;
            }
        }
    }

    const bool usedFallback = (bestTri < 0);
    if (usedFallback)
    {
        // No triplet encloses this finite direction. Measured never to happen
        // for finite input on a list built the way RenderEngine builds it (D-06;
        // the [ear][coverage] and [ear][gap] tests), but a caller can pass a
        // partial list (for example buildVBAPTripletsForLayout alone, which has
        // no below-horizon coverage). This used to be a Debug-only jassertfalse
        // on the audio thread; that is gone (WR-03): JUCE's assertion path logs
        // and allocates, and the outcome below is defined and audible instead.

        if (fallbackTri < 0)
            return;   // empty triplet list: silence

        // Use the largest-minimum-gain triplet, negatives clamped to 0; the
        // output mapping below (power scale or nadir downmix) then treats it
        // exactly like a found triplet of its kind.
        bestTri = fallbackTri;
        bestG[0] = std::max (0.0f, fallbackG[0]);
        bestG[1] = std::max (0.0f, fallbackG[1]);
        bestG[2] = std::max (0.0f, fallbackG[2]);
    }

    // bestTri >= 0 from here on: a found triplet or the D-06b candidate.
    if (triplets[static_cast<size_t> (bestTri)].kind() == VBAPTriplet::Kind::regular)
    {
        float power = bestG[0] * bestG[0] + bestG[1] * bestG[1] + bestG[2] * bestG[2];
        float scale = (power > 1e-12f) ? (1.0f / std::sqrt (power)) : 0.0f;

        outGains[triplets[static_cast<size_t> (bestTri)].i] = bestG[0] * scale;
        outGains[triplets[static_cast<size_t> (bestTri)].j] = bestG[1] * scale;
        outGains[triplets[static_cast<size_t> (bestTri)].k] = bestG[2] * scale;
    }
    else
    {
        // Lower-hemisphere triplet (D-04): each slot's gain accumulates onto
        // the real ear-level speaker it downmixes to (duplicates are legal);
        // the virtual nadir's gain is spread over nadirMask, then the result
        // is power-normalised. Stack floats and a const vector only (DR-1).
        const auto& tri = triplets[static_cast<size_t> (bestTri)];
        const int slot[3] = { tri.i, tri.j, tri.k };

        for (int v = 0; v < 3; ++v)
        {
            if (v == tri.nadirVertex)
            {
                const float share = bestG[v] * tri.nadirGain;
                for (int s = 0; s < N; ++s)
                    if ((tri.nadirMask >> s) & 1u)
                        outGains[s] += share;
            }
            else
            {
                outGains[slot[v]] += bestG[v];
            }
        }

        float power = 0.0f;
        for (int s = 0; s < N; ++s)
            power += outGains[s] * outGains[s];

        if (power > 1e-12f)
        {
            const float scale = 1.0f / std::sqrt (power);
            for (int s = 0; s < N; ++s)
                outGains[s] *= scale;
        }
    }

    // WR-03: the D-06b candidate can clamp to all zeros (every gain of the
    // best triplet negative). A finite direction with a non-empty list must
    // never be silent, so that case takes the nearest real speaker at unity,
    // the pre-Phase-2 fallback (d43cb15). Only reachable from the fallback
    // path; stack floats only (DR-1).
    if (usedFallback)
    {
        float power = 0.0f;
        for (int s = 0; s < N; ++s)
            power += outGains[s] * outGains[s];

        if (! (power > 1e-12f) && N > 0)
        {
            float bestDot = -2.0f;
            int bestSpeaker = 0;
            for (int s = 0; s < N; ++s)
            {
                outGains[s] = 0.0f;
                const float sx = std::cos (layout.speakers[s].elevationRad) * std::sin (layout.speakers[s].azimuthRad);
                const float sy = std::cos (layout.speakers[s].elevationRad) * std::cos (layout.speakers[s].azimuthRad);
                const float sz = std::sin (layout.speakers[s].elevationRad);
                const float dot = px * sx + py * sy + pz * sz;
                if (dot > bestDot)
                {
                    bestDot = dot;
                    bestSpeaker = s;
                }
            }
            outGains[bestSpeaker] = 1.0f;
        }
    }
}

} // namespace spatialcore
