#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>

#include <cmath>
#include <type_traits>
#include <vector>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// VBAP triplet selection: EAR lower hemisphere (D-04, D-05), above-horizon
// bit-identity against the pre-change function (D-04, D-06b), and the DR-3
// consumer-surface pins (D-02b).
// ============================================================================

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 64;

const LayoutID kHeightLayouts[] = {
    LayoutID::S5_1_2, LayoutID::S5_1_4, LayoutID::S7_1_2, LayoutID::S7_1_4,
    LayoutID::S7_1_6, LayoutID::S9_1_4, LayoutID::S9_1_6, LayoutID::SML13_1
};

const char* layoutName (LayoutID id)
{
    switch (id)
    {
        case LayoutID::S5_1_2:  return "5.1.2";
        case LayoutID::S5_1_4:  return "5.1.4";
        case LayoutID::S7_1_2:  return "7.1.2";
        case LayoutID::S7_1_4:  return "7.1.4";
        case LayoutID::S7_1_6:  return "7.1.6";
        case LayoutID::S9_1_4:  return "9.1.4";
        case LayoutID::S9_1_6:  return "9.1.6";
        case LayoutID::SML13_1: return "SML13.1";
        default:                return "?";
    }
}

/** Verbatim body of computeVBAPGains3D from git blob
    d43cb15:src/Core/SpatialMath.cpp lines 223-291 (the pre-change function,
    including its nearest-speaker fallback), with only the name changed. It is
    the oracle for the above-horizon bit-identity test. */
void referenceVBAPGains3D_d43cb15 (const SpeakerLayout& layout,
                         const std::vector<VBAPTriplet>& triplets,
                         float azimuthRad, float elevationRad,
                         float* outGains)
{
    const int N = layout.numSpeakers;
    for (int s = 0; s < N; ++s)
        outGains[s] = 0.0f;

    // Source direction as unit Cartesian vector
    float px = std::cos (elevationRad) * std::sin (azimuthRad);
    float py = std::cos (elevationRad) * std::cos (azimuthRad);
    float pz = std::sin (elevationRad);

    float bestGainSum = 1e30f;   // Start high -- pick MINIMUM sum (tightest enclosing triangle)
    int bestTri = -1;
    float bestG[3] = {};

    for (int t = 0; t < static_cast<int> (triplets.size()); ++t)
    {
        const auto& tri = triplets[static_cast<size_t> (t)];

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
    }

    if (bestTri >= 0)
    {
        float power = bestG[0] * bestG[0] + bestG[1] * bestG[1] + bestG[2] * bestG[2];
        float scale = (power > 1e-12f) ? (1.0f / std::sqrt (power)) : 0.0f;

        outGains[triplets[static_cast<size_t> (bestTri)].i] = bestG[0] * scale;
        outGains[triplets[static_cast<size_t> (bestTri)].j] = bestG[1] * scale;
        outGains[triplets[static_cast<size_t> (bestTri)].k] = bestG[2] * scale;
    }
    else
    {
        // Fallback: nearest speaker
        float bestDot = -2.0f;
        int bestSpeaker = 0;
        for (int s = 0; s < N; ++s)
        {
            float sx = std::cos (layout.speakers[s].elevationRad) * std::sin (layout.speakers[s].azimuthRad);
            float sy = std::cos (layout.speakers[s].elevationRad) * std::cos (layout.speakers[s].azimuthRad);
            float sz = std::sin (layout.speakers[s].elevationRad);
            float dot = px * sx + py * sy + pz * sz;
            if (dot > bestDot)
            {
                bestDot = dot;
                bestSpeaker = s;
            }
        }
        outGains[bestSpeaker] = 1.0f;
    }
}

std::vector<VBAPTriplet> regularOnly (const std::vector<VBAPTriplet>& all)
{
    std::vector<VBAPTriplet> out;
    for (const auto& t : all)
        if (! t.lowerHemisphere)
            out.push_back (t);
    return out;
}

/** Same arithmetic as the reference: does any regular triplet have all three
    gains at or above -1e-6f? */
bool regularTripletContains (const std::vector<VBAPTriplet>& regular, float azRad, float elRad)
{
    const float px = std::cos (elRad) * std::sin (azRad);
    const float py = std::cos (elRad) * std::cos (azRad);
    const float pz = std::sin (elRad);

    for (const auto& tri : regular)
    {
        const float g0 = tri.inv[0][0] * px + tri.inv[0][1] * py + tri.inv[0][2] * pz;
        const float g1 = tri.inv[1][0] * px + tri.inv[1][1] * py + tri.inv[1][2] * pz;
        const float g2 = tri.inv[2][0] * px + tri.inv[2][1] * py + tri.inv[2][2] * pz;
        if (g0 >= -1e-6f && g1 >= -1e-6f && g2 >= -1e-6f)
            return true;
    }
    return false;
}

/** A RenderEngine prepared for one output format, plus the 4-member
    LayoutContext a consumer builds from getActiveLayout(). */
struct EngineRig
{
    RenderEngine engine;

    explicit EngineRig (OutputFormat format)
    {
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (format);
    }

    LayoutContext context() const
    {
        const auto& ls = engine.getActiveLayout();
        return LayoutContext { ls.layout, ls.vbapTriplets, ls.ambiDecodeMatrix, ls.ambiNumSpeakers };
    }

    int numSpeakers() const { return engine.getActiveLayout().layout.numSpeakers; }

    void gainsAt (float azDeg, float elDeg, float* out) const
    {
        for (int s = 0; s < MAX_SPEAKERS; ++s)
            out[s] = 0.0f;
        const SourcePosition src { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), 0.5f };
        VBAPAlgorithm().computeGains (src, context(), out, numSpeakers());
    }
};

float powerOf (const float* g, int n)
{
    float p = 0.0f;
    for (int s = 0; s < n; ++s)
        p += g[s] * g[s];
    return p;
}
} // namespace

// ----------------------------------------------------------------------------
// Tracer: EAR lower hemisphere on 7.1.4 through the real engine and algorithm
// ----------------------------------------------------------------------------

TEST_CASE ("EAR tracer: nadir on 7.1.4 through RenderEngine and VBAPAlgorithm spreads 1/sqrt(7) over the ear-level ring (D-04)",
           "[ear][tracer]")
{
    EngineRig rig (OutputFormat::Surround7_1_4);
    REQUIRE (rig.numSpeakers() == 11);

    float g[MAX_SPEAKERS] = {};
    rig.gainsAt (30.0f, -90.0f, g);

    const float expected = 0.37796447f;   // 1 / sqrt (7)
    for (int s = 0; s <= 6; ++s)
    {
        INFO ("ear-level speaker " << s);
        CHECK_THAT (g[s], WithinAbs (expected, 1e-5f));
    }
    for (int s = 7; s <= 10; ++s)
    {
        INFO ("height speaker " << s);
        CHECK_THAT (g[s], WithinAbs (0.0f, 1e-6f));
    }

    bool anyLower = false;
    for (const auto& t : rig.engine.getActiveLayout().vbapTriplets)
        anyLower = anyLower || t.lowerHemisphere;
    CHECK (anyLower);
}

TEST_CASE ("EAR tracer: a source directly under the 30-degree speaker gets unity on it (D-04 meridian)",
           "[ear][tracer]")
{
    EngineRig rig (OutputFormat::Surround7_1_4);

    float g[MAX_SPEAKERS] = {};
    rig.gainsAt (30.0f, -15.0f, g);

    CHECK_THAT (g[0], WithinAbs (1.0f, 1e-5f));
    for (int s = 1; s < rig.numSpeakers(); ++s)
    {
        INFO ("speaker " << s);
        CHECK_THAT (g[s], WithinAbs (0.0f, 1e-5f));
    }
}

TEST_CASE ("EAR tracer: a below-horizon source never reaches an elevated speaker on 7.1.4 (D-04, D-05)",
           "[ear][tracer]")
{
    EngineRig rig (OutputFormat::Surround7_1_4);

    const float azimuths[]   = { 0.0f, 60.0f, 120.0f, 180.0f, -150.0f, -90.0f };
    const float elevations[] = { -20.0f, -45.0f, -70.0f };

    for (float az : azimuths)
        for (float el : elevations)
        {
            float g[MAX_SPEAKERS] = {};
            rig.gainsAt (az, el, g);

            INFO ("az " << az << " el " << el);
            for (int s = 7; s <= 10; ++s)
                CHECK (g[s] == 0.0f);
            CHECK_THAT (powerOf (g, rig.numSpeakers()), WithinAbs (1.0f, 1e-5f));
        }
}

// ----------------------------------------------------------------------------
// Above the horizon the new selection must be bit-identical to d43cb15
// ----------------------------------------------------------------------------

TEST_CASE ("VBAP 3D: above-horizon output is bit-identical to the pre-change function on every height layout (D-04, D-06b)",
           "[vbap3d-identity]")
{
    for (LayoutID id : kHeightLayouts)
    {
        const SpeakerLayout& layout = getLayoutDef (id);

        std::vector<VBAPTriplet> regular;
        buildVBAPTripletsForLayout (layout, regular);
        REQUIRE (! regular.empty());

        std::vector<VBAPTriplet> combined = regular;
        appendLowerHemisphereTriplets (layout, combined);
        REQUIRE (combined.size() > regular.size());

        long mismatches = 0;

        auto probe = [&] (float azDeg, float elDeg)
        {
            const float az = juce::degreesToRadians (azDeg);
            const float el = juce::degreesToRadians (elDeg);

            // The pre-change fallback must never be hit above the horizon.
            REQUIRE (regularTripletContains (regular, az, el));

            float expected[MAX_SPEAKERS] = {};
            float actual[MAX_SPEAKERS]   = {};
            referenceVBAPGains3D_d43cb15 (layout, regular, az, el, expected);
            computeVBAPGains3D (layout, combined, az, el, actual);

            for (int s = 0; s < layout.numSpeakers; ++s)
                if (! (actual[s] == expected[s]))
                {
                    ++mismatches;
                    INFO ("layout " << layoutName (id) << " az " << azDeg << " el " << elDeg
                          << " speaker " << s << " actual " << actual[s] << " expected " << expected[s]);
                    CHECK (actual[s] == expected[s]);
                }
        };

        for (int k = 0; k < 360; ++k)
        {
            const float azDeg = -179.5f + static_cast<float> (k);
            for (int j = 0; j < 90; ++j)
                probe (azDeg, 0.5f + static_cast<float> (j));
            probe (azDeg, 90.0f);
        }

        INFO ("layout " << layoutName (id));
        CHECK (mismatches == 0);
    }
}

// ----------------------------------------------------------------------------
// DR-3: the public surface OpenSpatialDelay compiles against is unchanged.
// These are compile-time pins first (a signature change breaks the build) and
// runtime checks second.
// ----------------------------------------------------------------------------

TEST_CASE ("DR-3: the public surface OpenSpatialDelay compiles against is unchanged (D-02b)",
           "[consumer-surface]")
{
    // setOutputFormat stays void (D-02b).
    static_assert (std::is_same_v<decltype (&RenderEngine::setOutputFormat),
                                  void (RenderEngine::*) (OutputFormat)>,
                   "RenderEngine::setOutputFormat must stay void (OutputFormat)");

    // VBAPTriplet built field by field, the way OSD builds it; the new members
    // keep their default initialisers.
    VBAPTriplet tri;
    tri.i = 1;
    tri.j = 2;
    tri.k = 3;
    tri.inv[0][0] = 1.0f;
    CHECK (tri.i == 1);
    CHECK (tri.inv[0][0] == 1.0f);
    CHECK_FALSE (tri.lowerHemisphere);
    CHECK (tri.nadirVertex == -1);

    // LayoutContext stays a 4-member aggregate OSD brace-initialises.
    SpeakerLayout layout {};
    std::vector<VBAPTriplet> triplets;
    float ambi[MAX_SPEAKERS][MAX_SPEAKERS] = {};
    const LayoutContext ctx { layout, triplets, ambi, 0 };
    CHECK (ctx.ambiNumSpeakers == 0);
    CHECK (ctx.triplets.empty());

    // Free functions keep their exact signatures.
    float (*evalSHFn) (int, float, float) = &evalSH;
    float (*evaluateSHFn) (int, float, float) = &AmbisonicsCodec::evaluateSH;
    void (*nearestFn) (const SpeakerLayout&, float, float, float*, int) = &nearestSpeaker3DFallback;
    void (*vbap2dFn) (const SpeakerLayout&, float, float*) = &computeVBAPGains2D;
    void (*vbap3dFn) (const SpeakerLayout&, const std::vector<VBAPTriplet>&, float, float, float*)
        = &computeVBAPGains3D;
    bool (*heightFn) (const SpeakerLayout&) = &layoutHasHeight;
    void (*buildFn) (const SpeakerLayout&, std::vector<VBAPTriplet>&) = &buildVBAPTripletsForLayout;

    CHECK (evalSHFn != nullptr);
    CHECK (evaluateSHFn != nullptr);
    CHECK (nearestFn != nullptr);
    CHECK (vbap2dFn != nullptr);
    CHECK (vbap3dFn != nullptr);
    CHECK (heightFn != nullptr);
    CHECK (buildFn != nullptr);
}
