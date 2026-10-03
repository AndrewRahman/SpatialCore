#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include "../reference/EarReference.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
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

/** Instantiate one of every algorithm named in AllAlgorithmTypes (copied from
    tests/Core/CountsTests.cpp). The pack is deduced from the header's own list,
    so a 9th algorithm is swept automatically. Test scope only. */
template <typename... Algorithms>
std::vector<std::unique_ptr<SpatializationAlgorithm>>
instantiateAll (AlgorithmTypeList<Algorithms...>)
{
    std::vector<std::unique_ptr<SpatializationAlgorithm>> out;
    out.reserve (sizeof...(Algorithms));
    (out.emplace_back (std::make_unique<Algorithms>()), ...);
    return out;
}

/** Run fn on its own thread and wait up to limit for it to finish. Returns true
    if it finished (the thread is joined) and false if it timed out (the thread
    is detached and left running). std::async is deliberately not used: its
    future blocks in the destructor on a hung task, which would turn a
    regression into a hung test binary instead of a failure. fn must only touch
    state it owns through a captured std::shared_ptr, so a detached, stuck thread
    never reads or writes freed memory. */
template <typename Fn>
bool runWithWatchdog (Fn fn, std::chrono::milliseconds limit)
{
    auto done = std::make_shared<std::atomic<bool>> (false);
    std::thread worker ([fn = std::move (fn), done]() mutable
    {
        fn();
        done->store (true);
    });

    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (! done->load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));

    if (done->load())
    {
        worker.join();
        return true;
    }

    worker.detach();
    return false;
}

bool allFiniteGains (const float* g, int n)
{
    for (int s = 0; s < n; ++s)
        if (! std::isfinite (g[s]))
            return false;
    return true;
}

bool allZeroGains (const float* g, int n)
{
    for (int s = 0; s < n; ++s)
        if (g[s] != 0.0f)
            return false;
    return true;
}

/** The D-06b rule, written independently of computeVBAPGains3D for regular
    triplets: the triplet with the largest min (g0, g1, g2) over the whole list
    (first one wins a tie), negatives clamped to 0, power-normalised. */
void largestMinGainReference (const std::vector<VBAPTriplet>& triplets,
                              float azRad, float elRad, float* out, int numSpeakers)
{
    for (int s = 0; s < numSpeakers; ++s)
        out[s] = 0.0f;

    const float px = std::cos (elRad) * std::sin (azRad);
    const float py = std::cos (elRad) * std::cos (azRad);
    const float pz = std::sin (elRad);

    int best = -1;
    float bestMin = -std::numeric_limits<float>::infinity();
    float g[3] = {};
    for (size_t t = 0; t < triplets.size(); ++t)
    {
        const auto& tri = triplets[t];
        const float g0 = tri.inv[0][0] * px + tri.inv[0][1] * py + tri.inv[0][2] * pz;
        const float g1 = tri.inv[1][0] * px + tri.inv[1][1] * py + tri.inv[1][2] * pz;
        const float g2 = tri.inv[2][0] * px + tri.inv[2][1] * py + tri.inv[2][2] * pz;
        const float mn = std::min (g0, std::min (g1, g2));
        if (mn > bestMin)
        {
            bestMin = mn;
            best = static_cast<int> (t);
            g[0] = std::max (0.0f, g0);
            g[1] = std::max (0.0f, g1);
            g[2] = std::max (0.0f, g2);
        }
    }

    if (best < 0)
        return;

    const float power = g[0] * g[0] + g[1] * g[1] + g[2] * g[2];
    const float scale = (power > 1e-12f) ? 1.0f / std::sqrt (power) : 0.0f;
    const auto& tri = triplets[static_cast<size_t> (best)];
    out[tri.i] = g[0] * scale;
    out[tri.j] = g[1] * scale;
    out[tri.k] = g[2] * scale;
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

TEST_CASE ("EAR band tracer: 7.1.4 at azimuth +/-60 holds 0.7071 / 0.7071 from 0 to -30 degrees through RenderEngine and VBAPAlgorithm (D-04, G-02-2)",
           "[ear][band][g02-2]")
{
    EngineRig rig (OutputFormat::Surround7_1_4);
    REQUIRE (rig.numSpeakers() == 11);

    const float elevations[] = { 0.0f, -5.0f, -10.0f, -15.0f, -20.0f, -25.0f, -30.0f };
    const float half = 0.70710678f;

    for (float el : elevations)
    {
        // Azimuth +60: M+030 / M+090 are speakers 0 and 3.
        {
            float g[MAX_SPEAKERS] = {};
            rig.gainsAt (60.0f, el, g);
            for (int s = 0; s < rig.numSpeakers(); ++s)
            {
                INFO ("az 60 el " << el << " speaker " << s);
                const float want = (s == 0 || s == 3) ? half : 0.0f;
                CHECK_THAT (g[s], WithinAbs (want, 1e-5f));
            }
            if (el <= -5.0f)
                for (int s = 7; s <= 10; ++s)
                {
                    INFO ("az 60 el " << el << " height speaker " << s);
                    CHECK (g[s] == 0.0f);
                }
        }

        // Azimuth -60 mirrors it on speakers 1 and 4.
        {
            float g[MAX_SPEAKERS] = {};
            rig.gainsAt (-60.0f, el, g);
            for (int s = 0; s < rig.numSpeakers(); ++s)
            {
                INFO ("az -60 el " << el << " speaker " << s);
                const float want = (s == 1 || s == 4) ? half : 0.0f;
                CHECK_THAT (g[s], WithinAbs (want, 1e-5f));
            }
            if (el <= -5.0f)
                for (int s = 7; s <= 10; ++s)
                {
                    INFO ("az -60 el " << el << " height speaker " << s);
                    CHECK (g[s] == 0.0f);
                }
        }
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
    CHECK (tri.kind() == VBAPTriplet::Kind::regular);

    // WR-02: the documented lower-hemisphere encoding, field by field. A
    // non-zero nadirMask is a cap; a zero one is a pair-pan wedge.
    VBAPTriplet cap;
    cap.lowerHemisphere = true;
    cap.nadirVertex = 2;
    cap.nadirMask = 0x7;
    cap.nadirGain = 0.57735f;
    CHECK (cap.kind() == VBAPTriplet::Kind::nadirCap);
    VBAPTriplet wedge = cap;
    wedge.nadirMask = 0;
    wedge.nadirGain = 0.0f;
    CHECK (wedge.kind() == VBAPTriplet::Kind::pairWedge);

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

// ----------------------------------------------------------------------------
// D-06 / D-19 robustness: no position value may hang an algorithm or make it
// emit a non-finite gain. Every batch runs under a 2-second watchdog, because
// before the D-19 fix computeVBAPGains2D looped forever on +inf and on any
// azimuth of about 1e9 rad (RESEARCH F7).
// ----------------------------------------------------------------------------

namespace
{
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

// Static storage: a detached (stuck) worker can still read these safely.
const float kRobustAzimuths[]   = { kNaN, kInf, -kInf, 1e30f, -1e30f, 1e9f, -1e9f, 1e6f, 0.3f };
const float kRobustElevations[] = { 0.0f, kNaN, kInf, -kInf, 1e30f, -0.4f };
const float kRobustDistances[]  = { 0.5f, kNaN };

constexpr auto kWatchdogLimit = std::chrono::milliseconds (2000);

struct RobustRecord
{
    int algo = 0;
    float az = 0.0f, el = 0.0f, dist = 0.0f;
    float g[MAX_SPEAKERS] = {};
    bool hasBinaural = false;
    BinauralGains bg;
};

/** Everything a robustness worker touches, owned through one shared_ptr. */
struct RobustState
{
    EngineRig rig;
    std::vector<std::unique_ptr<SpatializationAlgorithm>> algos;
    std::vector<RobustRecord> records;

    explicit RobustState (OutputFormat format)
        : rig (format), algos (instantiateAll (AllAlgorithmTypes{}))
    {
        records.reserve (algos.size() * std::size (kRobustAzimuths)
                         * std::size (kRobustElevations) * std::size (kRobustDistances));
    }
};

bool isVBAPFamilyOrKNN (const SpatializationAlgorithm& a)
{
    return dynamic_cast<const VBAPAlgorithm*> (&a) != nullptr
        || dynamic_cast<const VBIPAlgorithm*> (&a) != nullptr
        || dynamic_cast<const MDAPAlgorithm*> (&a) != nullptr
        || dynamic_cast<const KNNAlgorithm*> (&a) != nullptr;
}

bool binauralAllZero (const BinauralGains& b)
{
    return b.leftGain == 0.0f && b.rightGain == 0.0f
        && b.leftDelaySamples == 0.0f && b.rightDelaySamples == 0.0f;
}
} // namespace

TEST_CASE ("Robustness: every algorithm returns finite gains or silence and never hangs for non-finite or huge positions (D-06, D-19)",
           "[robust]")
{
    for (OutputFormat format : { OutputFormat::Quad, OutputFormat::Surround7_1_4 })
    {
        auto state = std::make_shared<RobustState> (format);
        const int n = state->rig.numSpeakers();
        const bool is3D = ! state->rig.engine.getActiveLayout().vbapTriplets.empty();
        REQUIRE (state->algos.size() == static_cast<size_t> (NUM_ALGORITHMS));
        REQUIRE (is3D == (format == OutputFormat::Surround7_1_4));

        const bool finished = runWithWatchdog ([state]
        {
            const LayoutContext ctx = state->rig.context();
            const int numSpk = state->rig.numSpeakers();
            const BinauralContext bctx { 1, kSampleRate, kDefaultBinauralProfiles };

            for (size_t a = 0; a < state->algos.size(); ++a)
                for (float az : kRobustAzimuths)
                    for (float el : kRobustElevations)
                        for (float dist : kRobustDistances)
                        {
                            RobustRecord r;
                            r.algo = static_cast<int> (a);
                            r.az = az; r.el = el; r.dist = dist;
                            const SourcePosition src { az, el, dist };
                            state->algos[a]->computeGains (src, ctx, r.g, numSpk);
                            if (state->algos[a]->supportsBinauralDirect())
                            {
                                r.hasBinaural = true;
                                r.bg = state->algos[a]->computeBinauralGains (src, bctx);
                            }
                            state->records.push_back (r);
                        }
        }, kWatchdogLimit);

        INFO ("format " << (is3D ? "7.1.4" : "Quad"));
        REQUIRE (finished);   // a hang here is the F7 infinite wrap loop
        REQUIRE (state->records.size() == state->algos.size() * std::size (kRobustAzimuths)
                                          * std::size (kRobustElevations) * std::size (kRobustDistances));

        bool sawBinaural = false;
        for (const auto& r : state->records)
        {
            const auto& algo = *state->algos[static_cast<size_t> (r.algo)];
            INFO (algo.getName().toStdString() << " az " << r.az << " el " << r.el << " dist " << r.dist);

            CHECK (allFiniteGains (r.g, n));
            const float p = powerOf (r.g, n);

            // Every algorithm, VBIP included since D-14 made it unit-power, is
            // either silent or at unit power.
            CHECK ((p == 0.0f || std::abs (p - 1.0f) <= 1e-4f));

            if (isVBAPFamilyOrKNN (algo))
            {
                const bool knn = dynamic_cast<const KNNAlgorithm*> (&algo) != nullptr;
                if (! std::isfinite (r.az))
                    CHECK (allZeroGains (r.g, n));
                if (! std::isfinite (r.el) && (is3D || knn))
                    CHECK (allZeroGains (r.g, n));
            }

            if (r.hasBinaural)
            {
                sawBinaural = true;
                CHECK (std::isfinite (r.bg.leftGain));
                CHECK (std::isfinite (r.bg.rightGain));
                CHECK (std::isfinite (r.bg.leftDelaySamples));
                CHECK (std::isfinite (r.bg.rightDelaySamples));
                if (! std::isfinite (r.az) || ! std::isfinite (r.el))
                    CHECK (binauralAllZero (r.bg));
            }
        }
        CHECK (sawBinaural);
    }
}

namespace
{
/** Everything the direct 2D/3D worker touches, owned through one shared_ptr. */
struct DirectVBAPState
{
    EngineRig quad { OutputFormat::Quad };
    EngineRig s714 { OutputFormat::Surround7_1_4 };

    float nanAz[MAX_SPEAKERS] = {}, posInfAz[MAX_SPEAKERS] = {}, negInfAz[MAX_SPEAKERS] = {};
    float huge1e9[MAX_SPEAKERS] = {}, huge1e30[MAX_SPEAKERS] = {};
    float at3_5[MAX_SPEAKERS] = {}, at3_5Wrapped[MAX_SPEAKERS] = {};
    float el3dNaN[MAX_SPEAKERS] = {}, el3dInf[MAX_SPEAKERS] = {};
    float az3dNaN[MAX_SPEAKERS] = {}, az3dInf[MAX_SPEAKERS] = {};
};

void fillSentinel (float* g)
{
    for (int s = 0; s < MAX_SPEAKERS; ++s)
        g[s] = 7.0f;
}
} // namespace

TEST_CASE ("computeVBAPGains2D/3D: non-finite gives silence, huge finite azimuths are wrapped not looped (D-19)",
           "[robust]")
{
    auto st = std::make_shared<DirectVBAPState>();
    const int nQuad = st->quad.numSpeakers();
    const int n714  = st->s714.numSpeakers();
    REQUIRE (st->quad.engine.getActiveLayout().vbapTriplets.empty());
    REQUIRE_FALSE (st->s714.engine.getActiveLayout().vbapTriplets.empty());

    // The float 2*pi the pre-change loop subtracted.
    const float twoPi = 2.0f * juce::MathConstants<float>::pi;

    const bool finished = runWithWatchdog ([st, twoPi]
    {
        const auto& quad = st->quad.engine.getActiveLayout().layout;
        float* outs[] = { st->nanAz, st->posInfAz, st->negInfAz, st->el3dNaN, st->el3dInf,
                          st->az3dNaN, st->az3dInf };
        for (float* g : outs)
            fillSentinel (g);

        computeVBAPGains2D (quad, kNaN,  st->nanAz);
        computeVBAPGains2D (quad, kInf,  st->posInfAz);
        computeVBAPGains2D (quad, -kInf, st->negInfAz);
        computeVBAPGains2D (quad, 1e9f,  st->huge1e9);
        computeVBAPGains2D (quad, 1e30f, st->huge1e30);
        computeVBAPGains2D (quad, 3.5f,  st->at3_5);
        computeVBAPGains2D (quad, 3.5f - twoPi, st->at3_5Wrapped);

        const auto& ls = st->s714.engine.getActiveLayout();
        computeVBAPGains3D (ls.layout, ls.vbapTriplets, 0.3f, kNaN, st->el3dNaN);
        computeVBAPGains3D (ls.layout, ls.vbapTriplets, 0.3f, kInf, st->el3dInf);
        computeVBAPGains3D (ls.layout, ls.vbapTriplets, kNaN, 0.2f, st->az3dNaN);
        computeVBAPGains3D (ls.layout, ls.vbapTriplets, kInf, 0.2f, st->az3dInf);
    }, kWatchdogLimit);

    REQUIRE (finished);   // a hang here is the F7 infinite wrap loop

    CHECK (allZeroGains (st->nanAz, nQuad));
    CHECK (allZeroGains (st->posInfAz, nQuad));
    CHECK (allZeroGains (st->negInfAz, nQuad));

    CHECK (allFiniteGains (st->huge1e9, nQuad));
    CHECK (allFiniteGains (st->huge1e30, nQuad));
    CHECK_THAT (powerOf (st->huge1e9, nQuad),  WithinAbs (1.0f, 1e-5f));
    CHECK_THAT (powerOf (st->huge1e30, nQuad), WithinAbs (1.0f, 1e-5f));

    // 3.5 rad was wrapped by one exact subtraction before; it still must be.
    for (int s = 0; s < nQuad; ++s)
    {
        INFO ("speaker " << s);
        CHECK (st->at3_5[s] == st->at3_5Wrapped[s]);
    }

    CHECK (allZeroGains (st->el3dNaN, n714));
    CHECK (allZeroGains (st->el3dInf, n714));
    CHECK (allZeroGains (st->az3dNaN, n714));
    CHECK (allZeroGains (st->az3dInf, n714));
}

TEST_CASE ("computeVBAPGains3D: no enclosing triplet uses the largest-minimum-gain triplet; an empty list is silent (D-06b)",
           "[robust]")
{
    // Both calls below take the no-triplet path. Since WR-03 that path has no
    // assert (it used to fire a Debug-only jassertfalse on the audio thread).
    const SpeakerLayout& layout = getLayoutDef (LayoutID::S7_1_4);
    const int n = layout.numSpeakers;

    std::vector<VBAPTriplet> regular;
    buildVBAPTripletsForLayout (layout, regular);
    REQUIRE_FALSE (regular.empty());
    for (const auto& t : regular)
        REQUIRE_FALSE (t.lowerHemisphere);

    // (30, -60) is the plan's probe; there the rule's answer happens to be unity
    // on one speaker, the same as the old nearest-speaker snap. (15, -10) and
    // (60, -10) are added because there the rule gives two non-zero gains
    // (measured 0.9717 / 0.2361 and 0.7532 / 0.6578), which no snap can produce.
    const float probesDeg[][2] = { { 30.0f, -60.0f }, { 15.0f, -10.0f }, { 60.0f, -10.0f } };
    int maxNonZero = 0;

    for (const auto& probe : probesDeg)
    {
        const float az = juce::degreesToRadians (probe[0]);
        const float el = juce::degreesToRadians (probe[1]);
        INFO ("az " << probe[0] << " el " << probe[1]);

        // Precondition: no regular triplet encloses this direction.
        REQUIRE_FALSE (regularTripletContains (regular, az, el));

        float expected[MAX_SPEAKERS] = {};
        float actual[MAX_SPEAKERS] = {};
        largestMinGainReference (regular, az, el, expected, n);
        computeVBAPGains3D (layout, regular, az, el, actual);

        CHECK (allFiniteGains (actual, n));
        CHECK_THAT (powerOf (actual, n), WithinAbs (1.0f, 1e-5f));
        int nonZero = 0;
        for (int s = 0; s < n; ++s)
        {
            INFO ("speaker " << s);
            CHECK (actual[s] >= 0.0f);
            CHECK_THAT (actual[s], WithinAbs (expected[s], 1e-6f));
            nonZero += (actual[s] != 0.0f) ? 1 : 0;
        }
        maxNonZero = std::max (maxNonZero, nonZero);
    }
    CHECK (maxNonZero >= 2);   // not a one-speaker snap

    // An empty triplet list has no candidate at all: silence.
    const std::vector<VBAPTriplet> none;
    float silent[MAX_SPEAKERS];
    fillSentinel (silent);
    computeVBAPGains3D (layout, none, juce::degreesToRadians (30.0f), juce::degreesToRadians (-60.0f), silent);
    CHECK (allZeroGains (silent, n));
}

TEST_CASE ("computeVBAPGains3D: a regular-only list is audible at every below-horizon direction, nearest speaker when the D-06b candidate clamps to silence (WR-03)",
           "[robust]")
{
    // A consumer that calls buildVBAPTripletsForLayout without
    // appendLowerHemisphereTriplets reaches the no-enclosing-triplet path for
    // every below-horizon direction. Before WR-03 that was a jassertfalse on
    // the audio thread in Debug and, where the D-06b candidate's three gains
    // were all negative, silence in Release.
    long totalDirections = 0, totalSnapped = 0;

    for (LayoutID id : kHeightLayouts)
    {
        const SpeakerLayout& layout = getLayoutDef (id);
        const int n = layout.numSpeakers;
        std::vector<VBAPTriplet> regular;
        buildVBAPTripletsForLayout (layout, regular);
        REQUIRE_FALSE (regular.empty());

        long directions = 0, snapped = 0, notUnit = 0, mismatches = 0;
        std::string firstFailure;
        auto noteFailure = [&] (const char* what, int azDeg, int elDeg, float value)
        {
            if (firstFailure.empty())
                firstFailure = std::string (what) + " at az " + std::to_string (azDeg)
                             + " el " + std::to_string (elDeg) + " value " + std::to_string (value);
        };

        for (int azDeg = -180; azDeg < 180; azDeg += 2)
        {
            for (int elDeg = -90; elDeg <= -2; elDeg += 2)
            {
                const float az = juce::degreesToRadians (static_cast<float> (azDeg));
                const float el = juce::degreesToRadians (static_cast<float> (elDeg));
                if (regularTripletContains (regular, az, el))
                    continue;   // not the fallback path
                ++directions;

                // Expected: the D-06b candidate, or the nearest-speaker snap
                // when that candidate clamps to all zeros.
                float expected[MAX_SPEAKERS] = {};
                largestMinGainReference (regular, az, el, expected, n);
                const bool clampedSilent = ! (powerOf (expected, n) > 1e-12f);
                if (clampedSilent)
                {
                    ++snapped;
                    nearestSpeaker3DFallback (layout, az, el, expected, n);
                }

                float g[MAX_SPEAKERS];
                fillSentinel (g);
                computeVBAPGains3D (layout, regular, az, el, g);

                const float p = powerOf (g, n);
                if (! (std::abs (p - 1.0f) <= 1e-5f))
                {
                    ++notUnit;
                    noteFailure ("power off unit (silent?)", azDeg, elDeg, p);
                }
                for (int s = 0; s < n; ++s)
                    if (! (std::abs (g[s] - expected[s]) <= (clampedSilent ? 0.0f : 1e-6f)))
                    {
                        ++mismatches;
                        noteFailure ("gain differs from the expected fallback", azDeg, elDeg, g[s]);
                    }
            }
        }

        INFO ("layout " << layoutName (id) << ": fallback directions " << directions
              << ", nearest-speaker snaps " << snapped << ", first failure: " << firstFailure);
        CHECK (directions > 0);
        CHECK (notUnit == 0);
        CHECK (mismatches == 0);
        totalDirections += directions;
        totalSnapped += snapped;
    }

    // The nearest-speaker branch must actually be exercised, or this test
    // would not catch its removal.
    INFO ("fallback directions " << totalDirections << ", nearest-speaker snaps " << totalSnapped);
    CHECK (totalSnapped > 0);
}

TEST_CASE ("DirectBinaural: non-finite direction gives silent binaural gains (D-06)", "[robust]")
{
    const DirectBinauralAlgorithm algo;
    const BinauralContext ctx { 1, kSampleRate, kDefaultBinauralProfiles };

    const SourcePosition bad[] = {
        { kNaN, 0.0f, 0.5f }, { kInf, 0.0f, 0.5f }, { -kInf, 0.0f, 0.5f },
        { 0.3f, kNaN, 0.5f }, { 0.3f, kInf, 0.5f }, { 0.3f, -kInf, 0.5f },
        { kNaN, kNaN, kNaN },
    };

    for (const auto& src : bad)
    {
        INFO ("az " << src.azimuthRad << " el " << src.elevationRad);
        const BinauralGains g = algo.computeBinauralGains (src, ctx);
        CHECK (g.leftGain == 0.0f);
        CHECK (g.rightGain == 0.0f);
        CHECK (g.leftDelaySamples == 0.0f);
        CHECK (g.rightDelaySamples == 0.0f);
    }

    // Sanity: a finite direction is not silenced.
    const BinauralGains ok = algo.computeBinauralGains ({ 0.3f, 0.1f, 0.5f }, ctx);
    CHECK (ok.leftGain > 0.0f);
    CHECK (std::isfinite (ok.rightDelaySamples));
}

// ----------------------------------------------------------------------------
// D-04 verification against PyPI ear 2.1.0 (the generated EAR oracle header)
// and full-sphere coverage: the lower hemisphere matches ear where ear is
// exact (the nadir cap), is unity under every ear-level speaker, is continuous
// across the horizon, and every sampled finite direction finds a triplet, so
// the D-06b fallback is a safety net and never a code path for finite input.
// ----------------------------------------------------------------------------

namespace
{
/** build + append, exactly what RenderEngine::activateLayout stores. */
std::vector<VBAPTriplet> combinedTriplets (const SpeakerLayout& layout)
{
    std::vector<VBAPTriplet> t;
    buildVBAPTripletsForLayout (layout, t);
    appendLowerHemisphereTriplets (layout, t);
    return t;
}

/** Test-local finder, same arithmetic as computeVBAPGains3D: does ANY triplet
    in the combined list have all three gains at or above -1e-6f? */
bool anyTripletContains (const std::vector<VBAPTriplet>& triplets, float azRad, float elRad)
{
    const float px = std::cos (elRad) * std::sin (azRad);
    const float py = std::cos (elRad) * std::cos (azRad);
    const float pz = std::sin (elRad);

    for (const auto& tri : triplets)
    {
        const float g0 = tri.inv[0][0] * px + tri.inv[0][1] * py + tri.inv[0][2] * pz;
        const float g1 = tri.inv[1][0] * px + tri.inv[1][1] * py + tri.inv[1][2] * pz;
        const float g2 = tri.inv[2][0] * px + tri.inv[2][1] * py + tri.inv[2][2] * pz;
        if (g0 >= -1e-6f && g1 >= -1e-6f && g2 >= -1e-6f)
            return true;
    }
    return false;
}

struct EarOracleSet
{
    LayoutID id;
    OutputFormat format;
    int numSpeakers;
    const spatialcore_ref::EarCase* cases;
    size_t numCases;
};
} // namespace

TEST_CASE ("EAR oracle: VBAP matches PyPI ear 2.1.0 in the nadir-cap region on 5.1.2, 7.1.4 and 9.1.6 (D-04)",
           "[ear][golden]")
{
    using namespace spatialcore_ref;
    const EarOracleSet sets[] = {
        { LayoutID::S5_1_2, OutputFormat::Surround5_1_2, kEarNumSpeakers_S5_1_2,
          kEarCases_S5_1_2, std::size (kEarCases_S5_1_2) },
        { LayoutID::S7_1_4, OutputFormat::Surround7_1_4, kEarNumSpeakers_S7_1_4,
          kEarCases_S7_1_4, std::size (kEarCases_S7_1_4) },
        { LayoutID::S9_1_6, OutputFormat::Surround9_1_6, kEarNumSpeakers_S9_1_6,
          kEarCases_S9_1_6, std::size (kEarCases_S9_1_6) },
    };

    size_t casesChecked = 0;
    for (const auto& set : sets)
    {
        EngineRig rig (set.format);
        const SpeakerLayout& layout = getLayoutDef (set.id);
        const auto triplets = combinedTriplets (layout);
        INFO ("layout " << layoutName (set.id));
        REQUIRE (rig.numSpeakers() == set.numSpeakers);
        REQUIRE (layout.numSpeakers == set.numSpeakers);

        for (size_t c = 0; c < set.numCases; ++c)
        {
            const auto& ec = set.cases[c];
            INFO ("az " << ec.azimuthDeg << " el " << ec.elevationDeg);

            float viaEngine[MAX_SPEAKERS] = {};
            rig.gainsAt (ec.azimuthDeg, ec.elevationDeg, viaEngine);

            float direct[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, juce::degreesToRadians (ec.azimuthDeg),
                                juce::degreesToRadians (ec.elevationDeg), direct);

            for (int s = 0; s < set.numSpeakers; ++s)
            {
                INFO ("speaker " << s);
                CHECK_THAT (viaEngine[s], WithinAbs (ec.gains[s], 1e-5f));
                CHECK_THAT (direct[s],    WithinAbs (ec.gains[s], 1e-5f));
            }
            ++casesChecked;
        }
    }
    CHECK (casesChecked == 13);
}

TEST_CASE ("EAR oracle: VBAP matches PyPI ear 2.1.0 in the below-horizon band on every height layout, 5.1.4's rear gap excluded (D-04, G-02-2)",
           "[ear][golden][band]")
{
    using namespace spatialcore_ref;
    const EarOracleSet sets[] = {
        { LayoutID::S5_1_2,  OutputFormat::Surround5_1_2,   kEarBandNumSpeakers_S5_1_2,
          kEarBandCases_S5_1_2,  std::size (kEarBandCases_S5_1_2) },
        { LayoutID::S5_1_4,  OutputFormat::Surround5_1_4,   kEarBandNumSpeakers_S5_1_4,
          kEarBandCases_S5_1_4,  std::size (kEarBandCases_S5_1_4) },
        { LayoutID::S7_1_2,  OutputFormat::Surround7_1_2,   kEarBandNumSpeakers_S7_1_2,
          kEarBandCases_S7_1_2,  std::size (kEarBandCases_S7_1_2) },
        { LayoutID::S7_1_4,  OutputFormat::Surround7_1_4,   kEarBandNumSpeakers_S7_1_4,
          kEarBandCases_S7_1_4,  std::size (kEarBandCases_S7_1_4) },
        { LayoutID::S7_1_6,  OutputFormat::Surround7_1_6,   kEarBandNumSpeakers_S7_1_6,
          kEarBandCases_S7_1_6,  std::size (kEarBandCases_S7_1_6) },
        { LayoutID::S9_1_4,  OutputFormat::Surround9_1_4,   kEarBandNumSpeakers_S9_1_4,
          kEarBandCases_S9_1_4,  std::size (kEarBandCases_S9_1_4) },
        { LayoutID::S9_1_6,  OutputFormat::Surround9_1_6,   kEarBandNumSpeakers_S9_1_6,
          kEarBandCases_S9_1_6,  std::size (kEarBandCases_S9_1_6) },
        { LayoutID::SML13_1, OutputFormat::SurroundSML13_1, kEarBandNumSpeakers_SML13_1,
          kEarBandCases_SML13_1, std::size (kEarBandCases_SML13_1) },
    };

    size_t casesChecked = 0;
    for (const auto& set : sets)
    {
        EngineRig rig (set.format);
        const SpeakerLayout& layout = getLayoutDef (set.id);
        const auto triplets = combinedTriplets (layout);
        INFO ("layout " << layoutName (set.id));
        REQUIRE (rig.numSpeakers() == set.numSpeakers);
        REQUIRE (layout.numSpeakers == set.numSpeakers);

        for (size_t c = 0; c < set.numCases; ++c)
        {
            const auto& ec = set.cases[c];
            INFO ("az " << ec.azimuthDeg << " el " << ec.elevationDeg);

            float viaEngine[MAX_SPEAKERS] = {};
            rig.gainsAt (ec.azimuthDeg, ec.elevationDeg, viaEngine);

            float direct[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, juce::degreesToRadians (ec.azimuthDeg),
                                juce::degreesToRadians (ec.elevationDeg), direct);

            for (int s = 0; s < set.numSpeakers; ++s)
            {
                INFO ("speaker " << s);
                CHECK_THAT (viaEngine[s], WithinAbs (ec.gains[s], 1e-5f));
                CHECK_THAT (direct[s],    WithinAbs (ec.gains[s], 1e-5f));
            }
            ++casesChecked;
        }
    }
    CHECK (casesChecked == 26);
}

namespace
{
/** Independent double-precision reference for the band: the 2D pair pan of the
    two azimuth-neighbouring ear-level speakers (|elevation| at most 10 degrees),
    from the source's horizontal direction only. All other speakers get 0. */
void earLevelPairPan (const SpeakerLayout& layout, double azRad, float* out)
{
    const double twoPi = 2.0 * 3.14159265358979323846;
    const double earLimit = 10.0 * 3.14159265358979323846 / 180.0;

    for (int s = 0; s < MAX_SPEAKERS; ++s)
        out[s] = 0.0f;

    std::vector<std::pair<double, int>> ear;   // (azimuth wrapped to [0, 2 pi), speaker)
    for (int s = 0; s < layout.numSpeakers; ++s)
    {
        if (std::abs (static_cast<double> (layout.speakers[s].elevationRad)) > earLimit)
            continue;
        double a = std::fmod (static_cast<double> (layout.speakers[s].azimuthRad), twoPi);
        if (a < 0.0)
            a += twoPi;
        ear.emplace_back (a, s);
    }
    std::sort (ear.begin(), ear.end());

    const size_t n = ear.size();
    for (size_t p = 0; p < n; ++p)
    {
        const auto& A = ear[p];
        const auto& B = ear[(p + 1) % n];

        // Solve [sin aA, sin aB; cos aA, cos aB] g = [sin az, cos az].
        const double m00 = std::sin (A.first), m01 = std::sin (B.first);
        const double m10 = std::cos (A.first), m11 = std::cos (B.first);
        const double det = m00 * m11 - m01 * m10;
        if (std::abs (det) < 1e-9)
            continue;

        const double rx = std::sin (azRad), ry = std::cos (azRad);
        const double gA = (rx * m11 - m01 * ry) / det;
        const double gB = (m00 * ry - rx * m10) / det;
        if (gA < -1e-9 || gB < -1e-9)
            continue;

        const double norm = std::sqrt (gA * gA + gB * gB);
        out[A.second] = static_cast<float> (std::max (0.0, gA) / norm);
        out[B.second] = static_cast<float> (std::max (0.0, gB) / norm);
        return;
    }
}

/** The production classifier (WR-02, IN-06): 0 regular, 1 nadir cap, 2 wedge. */
int testTier (const VBAPTriplet& t)
{
    return static_cast<int> (t.kind());
}

/** Counts, per tier, the triplets enclosing the direction with the code's own
    float arithmetic and -1e-6f test. */
void enclosingByTier (const std::vector<VBAPTriplet>& triplets, float azRad, float elRad, int* counts)
{
    counts[0] = counts[1] = counts[2] = 0;
    const float px = std::cos (elRad) * std::sin (azRad);
    const float py = std::cos (elRad) * std::cos (azRad);
    const float pz = std::sin (elRad);

    for (const auto& tri : triplets)
    {
        const float g0 = tri.inv[0][0] * px + tri.inv[0][1] * py + tri.inv[0][2] * pz;
        const float g1 = tri.inv[1][0] * px + tri.inv[1][1] * py + tri.inv[1][2] * pz;
        const float g2 = tri.inv[2][0] * px + tri.inv[2][1] * py + tri.inv[2][2] * pz;
        if (g0 >= -1e-6f && g1 >= -1e-6f && g2 >= -1e-6f)
            ++counts[testTier (tri)];
    }
}
} // namespace

TEST_CASE ("EAR band: below the horizon VBAP keeps the ear-level pair pan of its azimuth, with exactly one pair region enclosing, on every height layout (D-04, G-02-2)",
           "[ear][band]")
{
    const float elevations[] = { -0.5f, -1.0f, -2.0f, -5.0f, -10.0f, -15.0f, -20.0f, -25.0f, -29.5f };
    const double earLimit = 10.0 * 3.14159265358979323846 / 180.0;

    for (LayoutID id : kHeightLayouts)
    {
        const SpeakerLayout& layout = getLayoutDef (id);
        const auto triplets = combinedTriplets (layout);

        long directions = 0, gainViolations = 0, regionViolations = 0;
        std::string firstFailure;
        auto noteFailure = [&] (const char* what, float azDeg, float elDeg, float value)
        {
            if (firstFailure.empty())
                firstFailure = std::string (what) + " at az " + std::to_string (azDeg)
                             + " el " + std::to_string (elDeg) + " value " + std::to_string (value);
        };

        for (int k = 0; k < 720; ++k)
        {
            const float azDeg = -180.0f + 0.5f * static_cast<float> (k);
            const float az = juce::degreesToRadians (azDeg);

            // Within 0.01 degrees of an ear-level speaker's azimuth two
            // neighbouring wedges legitimately share an edge.
            bool nearSpeaker = false;
            for (int s = 0; s < layout.numSpeakers; ++s)
            {
                if (std::abs (static_cast<double> (layout.speakers[s].elevationRad)) > earLimit)
                    continue;
                double d = std::fmod (static_cast<double> (azDeg) - static_cast<double> (juce::radiansToDegrees (layout.speakers[s].azimuthRad)), 360.0);
                if (d > 180.0)  d -= 360.0;
                if (d < -180.0) d += 360.0;
                if (std::abs (d) < 0.01)
                    nearSpeaker = true;
            }

            for (float elDeg : elevations)
            {
                const float el = juce::degreesToRadians (elDeg);
                ++directions;

                float g[MAX_SPEAKERS] = {};
                float ref[MAX_SPEAKERS] = {};
                computeVBAPGains3D (layout, triplets, az, el, g);
                earLevelPairPan (layout, static_cast<double> (az), ref);

                float worst = 0.0f;
                for (int s = 0; s < layout.numSpeakers; ++s)
                    worst = std::max (worst, std::abs (g[s] - ref[s]));
                if (worst > 1e-5f)
                {
                    ++gainViolations;
                    noteFailure ("differs from the pair pan", azDeg, elDeg, worst);
                }

                int counts[3];
                enclosingByTier (triplets, az, el, counts);
                const bool badRegions = counts[0] > 0 || counts[1] > 0 || (! nearSpeaker && counts[2] != 1);
                if (badRegions)
                {
                    ++regionViolations;
                    noteFailure ("wrong enclosing regions (regular/cap/wedge)", azDeg, elDeg,
                                 static_cast<float> (counts[0] * 10000 + counts[1] * 100 + counts[2]));
                }
            }
        }

        INFO ("layout " << layoutName (id) << " first failure: " << firstFailure);
        CHECK (directions == 720L * 9L);
        CHECK (gainViolations == 0);
        CHECK (regionViolations == 0);
    }
}

TEST_CASE ("EAR: a source directly under any ear-level speaker of any height layout gets unity on it (D-04)",
           "[ear]")
{
    const float earLevelLimit = juce::degreesToRadians (10.0f);

    for (LayoutID id : kHeightLayouts)
    {
        const SpeakerLayout& layout = getLayoutDef (id);
        const auto triplets = combinedTriplets (layout);
        int earLevelSpeakers = 0;

        for (int spk = 0; spk < layout.numSpeakers; ++spk)
        {
            if (std::abs (layout.speakers[spk].elevationRad) > earLevelLimit)
                continue;
            ++earLevelSpeakers;

            float g[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, layout.speakers[spk].azimuthRad,
                                juce::degreesToRadians (-15.0f), g);

            INFO ("layout " << layoutName (id) << " speaker " << spk << " az "
                  << juce::radiansToDegrees (layout.speakers[spk].azimuthRad));
            for (int s = 0; s < layout.numSpeakers; ++s)
            {
                INFO ("gain on speaker " << s);
                CHECK_THAT (g[s], WithinAbs (s == spk ? 1.0f : 0.0f, 1e-5f));
            }
        }
        INFO ("layout " << layoutName (id));
        CHECK (earLevelSpeakers >= 5);
    }
}

TEST_CASE ("EAR: horizon continuity at every 0.1-degree azimuth on every height layout (D-04, F4)",
           "[ear][continuity]")
{
    const float above = juce::degreesToRadians (0.05f);
    const float below = juce::degreesToRadians (-0.05f);

    for (LayoutID id : kHeightLayouts)
    {
        const SpeakerLayout& layout = getLayoutDef (id);
        const auto triplets = combinedTriplets (layout);

        float maxStep = 0.0f;
        float worstAzDeg = 0.0f;
        for (int k = 0; k < 3600; ++k)
        {
            const float azDeg = -180.0f + 0.1f * static_cast<float> (k);
            const float az = juce::degreesToRadians (azDeg);

            float gUp[MAX_SPEAKERS] = {};
            float gDown[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, az, above, gUp);
            computeVBAPGains3D (layout, triplets, az, below, gDown);

            for (int s = 0; s < layout.numSpeakers; ++s)
            {
                const float step = std::abs (gUp[s] - gDown[s]);
                if (step > maxStep)
                {
                    maxStep = step;
                    worstAzDeg = azDeg;
                }
            }
        }

        // Run with -s to read the measured maximum per layout.
        INFO ("layout " << layoutName (id) << " max horizon step " << maxStep << " at az " << worstAzDeg);
        CHECK (maxStep <= 0.01f);
    }
}

TEST_CASE ("EAR: every finite direction resolves to a triplet with unit power on every height layout (D-04, D-06)",
           "[ear][coverage]")
{
    // The assumption-delta invariant recorded in Plan 02-01: for finite input
    // the combined regular + lower-hemisphere list always has an enclosing
    // triplet, so the D-06b fallback in computeVBAPGains3D is never reached.
    const float elevatedLimit = juce::degreesToRadians (10.0f);
    const float belowLimit    = juce::degreesToRadians (-1.0f);
    const float pi = juce::MathConstants<float>::pi;

    for (LayoutID id : kHeightLayouts)
    {
        const SpeakerLayout& layout = getLayoutDef (id);
        const auto triplets = combinedTriplets (layout);

        std::vector<std::pair<float, float>> dirs;   // (azRad, elRad)
        dirs.reserve (40000 + 180 * 91 + MAX_SPEAKERS + 2);

        std::mt19937_64 rng (42);
        std::uniform_real_distribution<double> zDist (-1.0, 1.0);
        std::uniform_real_distribution<double> azDist (-static_cast<double> (pi), static_cast<double> (pi));
        for (int i = 0; i < 40000; ++i)
        {
            const double z = zDist (rng);
            dirs.emplace_back (static_cast<float> (azDist (rng)), static_cast<float> (std::asin (z)));
        }
        for (int a = -180; a < 180; a += 2)
            for (int e = -90; e <= 90; e += 2)
                dirs.emplace_back (juce::degreesToRadians (static_cast<float> (a)),
                                   juce::degreesToRadians (static_cast<float> (e)));
        for (int s = 0; s < layout.numSpeakers; ++s)
            dirs.emplace_back (layout.speakers[s].azimuthRad, layout.speakers[s].elevationRad);
        dirs.emplace_back (0.0f,  pi * 0.5f);
        dirs.emplace_back (0.0f, -pi * 0.5f);

        long uncovered = 0, offUnit = 0, elevatedLeak = 0;
        std::string firstFailure;
        auto noteFailure = [&] (const char* what, float az, float el, float value)
        {
            if (firstFailure.empty())
                firstFailure = std::string (what) + " at az " + std::to_string (juce::radiansToDegrees (az))
                             + " el " + std::to_string (juce::radiansToDegrees (el))
                             + " value " + std::to_string (value);
        };

        for (const auto& [az, el] : dirs)
        {
            if (! anyTripletContains (triplets, az, el))
            {
                ++uncovered;
                noteFailure ("no enclosing triplet", az, el, 0.0f);
            }

            float g[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, az, el, g);

            const float p = powerOf (g, layout.numSpeakers);
            if (! (std::abs (p - 1.0f) <= 2e-6f))
            {
                ++offUnit;
                noteFailure ("power off unit", az, el, p);
            }

            if (el <= belowLimit)
                for (int s = 0; s < layout.numSpeakers; ++s)
                    if (layout.speakers[s].elevationRad > elevatedLimit && g[s] != 0.0f)
                    {
                        ++elevatedLeak;
                        noteFailure ("elevated speaker gain below the horizon", az, el, g[s]);
                    }
        }

        INFO ("layout " << layoutName (id) << " directions " << dirs.size() << " first failure: " << firstFailure);
        CHECK (dirs.size() == 40000 + 180 * 91 + static_cast<size_t> (layout.numSpeakers) + 2);
        CHECK (uncovered == 0);
        CHECK (offUnit == 0);
        CHECK (elevatedLeak == 0);
    }
}

// ----------------------------------------------------------------------------
// WR-05: an ear-level ring that does not surround the listener (an azimuth gap
// of 180 degrees or more between neighbouring ear-level speakers). Before the
// fix the hull orientation test dropped the wrapping pair's cap and wedge
// (4 extras instead of 6 on a 3-speaker ring) and below-horizon directions in
// the gap had no enclosing triplet: silent or near-silent in Release, an assert
// in Debug. These layouts are consumer-defined: no shipped layout has a gap
// that wide (the widest is 140 degrees, 5.x's rear).
// ----------------------------------------------------------------------------

namespace
{
SpeakerLayout makeLayoutDeg (const std::vector<std::pair<float, float>>& speakersDeg)
{
    SpeakerLayout l {};
    l.numSpeakers     = static_cast<int> (speakersDeg.size());
    l.lfeChannelIndex = -1;
    l.totalChannels   = l.numSpeakers;
    for (int s = 0; s < l.numSpeakers; ++s)
        l.speakers[s] = { juce::degreesToRadians (speakersDeg[static_cast<size_t> (s)].first),
                          juce::degreesToRadians (speakersDeg[static_cast<size_t> (s)].second), s };
    return l;
}

struct GapLayoutCase
{
    const char* name;
    std::vector<std::pair<float, float>> speakersDeg;   // (azimuth, elevation)
    int earLevel;      // n: speakers within 10 degrees of the horizon
    int bridgedGaps;   // g: ear-level azimuth gaps of 179 degrees or more
};
} // namespace

TEST_CASE ("EAR gap: an ear-level ring with an azimuth gap of 180 degrees or more still covers every below-horizon direction with sound (WR-05)",
           "[ear][gap]")
{
    const GapLayoutCase cases[] = {
        { "front-only ring 0/+30/-30 plus 3 heights (300-degree rear gap)",
          { { 0, 0 }, { 30, 0 }, { -30, 0 }, { 30, 45 }, { -30, 45 }, { 180, 45 } }, 3, 1 },
        { "ring 0/+90/-90 plus 2 heights (exactly 180-degree rear gap)",
          { { 0, 0 }, { 90, 0 }, { -90, 0 }, { 45, 45 }, { -45, 45 } }, 3, 1 },
        { "control: ring 0/+100/-100 plus 2 heights (160-degree gap, no bridge)",
          { { 0, 0 }, { 100, 0 }, { -100, 0 }, { 45, 45 }, { -45, 45 } }, 3, 0 },
    };

    const float elevatedLimit = juce::degreesToRadians (10.0f);

    for (const auto& c : cases)
    {
        const SpeakerLayout layout = makeLayoutDeg (c.speakersDeg);
        const int n = layout.numSpeakers;

        std::vector<VBAPTriplet> regular;
        buildVBAPTripletsForLayout (layout, regular);
        const auto triplets = combinedTriplets (layout);
        REQUIRE (triplets.size() >= regular.size());

        // Documented count: 2 (n + 2g) -- one cap and one wedge per
        // neighbouring pair of the ear-level ring, where each bridged gap adds
        // two virtual ear-level vertices to the ring.
        const size_t extras = triplets.size() - regular.size();

        // 5000 seeded random directions, uniform over the lower hemisphere
        // (the reviewer's measurement), plus every 2 degrees of a grid.
        std::vector<std::pair<float, float>> dirs;
        std::mt19937_64 rng (2025);
        std::uniform_real_distribution<double> zDist (-1.0, 0.0);
        std::uniform_real_distribution<double> azDist (-3.14159265358979323846, 3.14159265358979323846);
        for (int i = 0; i < 5000; ++i)
        {
            const double z = zDist (rng);
            dirs.emplace_back (static_cast<float> (azDist (rng)), static_cast<float> (std::asin (z)));
        }
        for (int a = -180; a < 180; a += 2)
            for (int e = -90; e < 0; e += 2)
                dirs.emplace_back (juce::degreesToRadians (static_cast<float> (a)),
                                   juce::degreesToRadians (static_cast<float> (e)));

        long silent = 0, offUnit = 0, uncovered = 0, elevatedLeak = 0;
        std::string firstFailure;
        auto noteFailure = [&] (const char* what, float az, float el, float value)
        {
            if (firstFailure.empty())
                firstFailure = std::string (what) + " at az " + std::to_string (juce::radiansToDegrees (az))
                             + " el " + std::to_string (juce::radiansToDegrees (el))
                             + " value " + std::to_string (value);
        };

        for (const auto& [az, el] : dirs)
        {
            if (! anyTripletContains (triplets, az, el))
            {
                ++uncovered;
                noteFailure ("no enclosing triplet", az, el, 0.0f);
            }

            float g[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, az, el, g);
            const float p = powerOf (g, n);

            // The reviewer's "silent" criterion, and the stricter unit-power one
            // (written so a NaN power counts as a failure).
            if (! (p >= 0.5f))
            {
                ++silent;
                noteFailure ("silent (power < 0.5)", az, el, p);
            }
            if (! (std::abs (p - 1.0f) <= 2e-6f))
            {
                ++offUnit;
                noteFailure ("power off unit", az, el, p);
            }
            for (int s = 0; s < n; ++s)
                if (layout.speakers[s].elevationRad > elevatedLimit && g[s] != 0.0f)
                {
                    ++elevatedLeak;
                    noteFailure ("elevated speaker gain below the horizon", az, el, g[s]);
                }
        }

        // Continuity: the bridge pans smoothly across the gap. Largest gain step
        // between neighbouring 0.1-degree samples, along azimuth at four
        // elevations and along elevation at five azimuths inside the rear gap.
        // A NaN step counts as a failure (WR-06 pattern).
        float maxStep = 0.0f;
        long nonFiniteSteps = 0;
        auto stepBetween = [&] (float az0, float el0, float az1, float el1)
        {
            float g0[MAX_SPEAKERS] = {};
            float g1[MAX_SPEAKERS] = {};
            computeVBAPGains3D (layout, triplets, az0, el0, g0);
            computeVBAPGains3D (layout, triplets, az1, el1, g1);
            for (int s = 0; s < n; ++s)
            {
                const float d = std::abs (g1[s] - g0[s]);
                if (! std::isfinite (d))
                {
                    ++nonFiniteSteps;
                    noteFailure ("non-finite gain step", az1, el1, d);
                }
                else if (d > maxStep)
                {
                    maxStep = d;
                    if (d > 0.01f)
                        noteFailure ("gain step above 0.01", az1, el1, d);
                }
            }
        };
        for (float elDeg : { -5.0f, -15.0f, -45.0f, -75.0f })
            for (int k = 0; k < 3600; ++k)
                stepBetween (juce::degreesToRadians (-180.0f + 0.1f * static_cast<float> (k)),
                             juce::degreesToRadians (elDeg),
                             juce::degreesToRadians (-180.0f + 0.1f * static_cast<float> (k + 1)),
                             juce::degreesToRadians (elDeg));
        for (float azDeg : { 120.0f, 150.0f, 180.0f, -150.0f, -120.0f })
            for (int k = 0; k < 899; ++k)
                stepBetween (juce::degreesToRadians (azDeg), juce::degreesToRadians (-0.05f - 0.1f * static_cast<float> (k)),
                             juce::degreesToRadians (azDeg), juce::degreesToRadians (-0.05f - 0.1f * static_cast<float> (k + 1)));

        INFO (c.name << ": extras " << extras << ", silent " << silent << ", uncovered " << uncovered
              << ", max step " << maxStep << ", first failure: " << firstFailure);
        CHECK (dirs.size() == 5000u + 180u * 45u);
        CHECK (extras == static_cast<size_t> (2 * (c.earLevel + 2 * c.bridgedGaps)));
        CHECK (silent == 0);
        CHECK (offUnit == 0);
        CHECK (uncovered == 0);
        CHECK (elevatedLeak == 0);
        CHECK (nonFiniteSteps == 0);
        CHECK (maxStep <= 0.01f);
    }
}
