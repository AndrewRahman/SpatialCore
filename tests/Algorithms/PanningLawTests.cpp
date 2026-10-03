#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>
#include <SpatialCore/Core/SpatialMath.h>
#include "../reference/PanningReference.h"
#include "../TestNumerics.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// D-13 panning-law suite: "verified against the panning laws" (ROADMAP overview
// line 27) means every algorithm in AllAlgorithmTypes is checked against its own
// laws -- on-speaker behaviour, unit power, left/right mirror symmetry and
// 360-degree continuity where the algorithm is continuous -- through the same
// LayoutContext a consumer builds from RenderEngine::getActiveLayout(), so
// Ambisonics sees the real order-3 decode and height layouts see the regular
// plus lower-hemisphere triplets. Textbook values come from
// the generated PanningReference.h header (published formulas), never from the code
// under test.
// ============================================================================

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 64;

/** Instantiate one of every algorithm named in AllAlgorithmTypes (copied from
    tests/Core/CountsTests.cpp). The pack is deduced from the header's own list,
    so there is no second hand-written list to forget. */
template <typename... Algorithms>
std::vector<std::unique_ptr<SpatializationAlgorithm>>
instantiateAll (AlgorithmTypeList<Algorithms...>)
{
    std::vector<std::unique_ptr<SpatializationAlgorithm>> out;
    out.reserve (sizeof...(Algorithms));
    (out.emplace_back (std::make_unique<Algorithms>()), ...);
    return out;
}

// ----------------------------------------------------------------------------
// Law table, keyed by getName(). Adding a ninth algorithm to AllAlgorithmTypes
// fails "every algorithm ... has declared laws" until a row is written here.
// ----------------------------------------------------------------------------
enum class OnSpeakerLaw
{
    Unity,            // gain 1 on that speaker, 0 elsewhere (VBAP, VBIP, KNN)
    NearUnity,        // >= 1 - 2e-3: d^2 is clamped at 0.001 (DBAP)
    ArgmaxBelowOne,   // largest gain, strictly below 1 -- spreads by design (MDAP, F3)
    Argmax,           // largest gain (ConstantPower)
    FiniteOnly,       // no on-speaker law for a mode-matching decode (Ambisonics)
    NotApplicable     // no speaker gains (DirectBinaural)
};

enum class MirrorRule
{
    Everywhere,           // flat layouts and height layouts at el 20 / 45
    TieFilteredOnHeight,  // flat layouts; height layouts only off coplanar ties (F5, #22)
    FlatOnly,             // flat layouts only
    KnnTieFiltered,       // all layouts, only where the 3rd/4th nearest do not tie
    NotApplicable
};

enum class ContinuityScope
{
    Everywhere,   // all 15 layouts at el 0, 20 and 40
    Horizon,      // all 15 layouts at el 0 only
    None          // inherent switching (KNN) or no speaker gains (DirectBinaural)
};

struct PanningLaws
{
    const char*     name;
    OnSpeakerLaw    onSpeaker;
    float           onSpeakerOthersTolHeight; // Unity only: tolerance on the other gains, height layouts
    float           powerTol;                 // |sum g^2 - 1|
    bool            mayBeSilentOffFlatHorizon; // Ambisonics: clamped decode may render silent
    MirrorRule      mirror;
    float           mirrorTolHeight;          // tolerance on height layouts (flat: 1e-4)
    ContinuityScope continuity;
    float           continuityBoundFlat;      // largest per-speaker step, flat layouts (all layouts for Everywhere)
    float           continuityBoundHeight;    // largest per-speaker step, height layouts at the horizon
    float           heightStepDeg;            // azimuth step on height layouts
};

const PanningLaws kLaws[] = {
    { "Constant Power",   OnSpeakerLaw::Argmax,         0.0f,  1e-4f, false, MirrorRule::Everywhere,          1e-4f, ContinuityScope::Everywhere, 0.02f, 0.02f, 0.1f },
    { "VBAP",             OnSpeakerLaw::Unity,          1e-5f, 1e-5f, false, MirrorRule::TieFilteredOnHeight, 1e-4f, ContinuityScope::Horizon,    0.01f, 0.01f, 0.1f },
    // VBIP: the square root turns ~1e-7 float noise of the 3D solve into ~3e-4, so
    // its other on-speaker gains and its height-layout mirror use 1e-3. Its
    // continuity bound is 0.08 -- see the continuity TEST_CASE for the derivation.
    { "VBIP",             OnSpeakerLaw::Unity,          1e-3f, 1e-5f, false, MirrorRule::TieFilteredOnHeight, 1e-3f, ContinuityScope::Horizon,    0.08f, 0.08f, 0.1f },
    { "KNN",              OnSpeakerLaw::Unity,          1e-5f, 1e-4f, false, MirrorRule::KnnTieFiltered,      1e-4f, ContinuityScope::None,       0.0f,  0.0f,  0.1f },
    { "DBAP",             OnSpeakerLaw::NearUnity,      0.0f,  1e-4f, false, MirrorRule::Everywhere,          1e-4f, ContinuityScope::Everywhere, 0.02f, 0.02f, 0.1f },
    // MDAP on height layouts at the horizon: 0.5-degree steps, at most 0.12 (was
    // 0.16-0.19 before D-04 gave the below-horizon aux directions real triplets).
    { "MDAP",             OnSpeakerLaw::ArgmaxBelowOne, 0.0f,  1e-4f, false, MirrorRule::FlatOnly,            1e-4f, ContinuityScope::Horizon,    0.01f, 0.12f, 0.5f },
    { "Ambisonics (HOA)", OnSpeakerLaw::FiniteOnly,     0.0f,  1e-4f, true,  MirrorRule::Everywhere,          1e-4f, ContinuityScope::Everywhere, 0.02f, 0.02f, 0.1f },
    // DirectBinaural has no speaker gains; its laws are the property checks in
    // "DirectBinaural: property checks at the horizon".
    { "Direct Binaural",  OnSpeakerLaw::NotApplicable,  0.0f,  0.0f,  false, MirrorRule::NotApplicable,       0.0f,  ContinuityScope::None,       0.0f,  0.0f,  0.1f },
};

const PanningLaws* lawsFor (const SpatializationAlgorithm& algo)
{
    const std::string name = algo.getName().toStdString();
    for (const auto& laws : kLaws)
        if (name == laws.name)
            return &laws;
    return nullptr;
}

// ----------------------------------------------------------------------------
// Engine rigs
// ----------------------------------------------------------------------------
struct FormatEntry { OutputFormat format; const char* name; };

const FormatEntry kFlatFormats[] = {
    { OutputFormat::Quad,        "Quad" },
    { OutputFormat::Surround5_0, "5.0" },
    { OutputFormat::Surround5_1, "5.1" },
    { OutputFormat::Surround7_0, "7.0" },
    { OutputFormat::Surround7_1, "7.1" },
    { OutputFormat::Surround9_1, "9.1" },
    { OutputFormat::Octaphonic,  "Octaphonic" },
};

const FormatEntry kHeightFormats[] = {
    { OutputFormat::Surround5_1_2,   "5.1.2" },
    { OutputFormat::Surround5_1_4,   "5.1.4" },
    { OutputFormat::Surround7_1_2,   "7.1.2" },
    { OutputFormat::Surround7_1_4,   "7.1.4" },
    { OutputFormat::Surround7_1_6,   "7.1.6" },
    { OutputFormat::Surround9_1_4,   "9.1.4" },
    { OutputFormat::Surround9_1_6,   "9.1.6" },
    { OutputFormat::SurroundSML13_1, "SML13.1" },
};

/** A RenderEngine prepared for one output format, plus the 4-member
    LayoutContext a consumer builds from getActiveLayout(). */
struct Rig
{
    const char* name;
    bool        height;
    RenderEngine engine;

    Rig (const FormatEntry& entry, bool isHeight) : name (entry.name), height (isHeight)
    {
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (entry.format);
    }

    LayoutContext context() const
    {
        const auto& ls = engine.getActiveLayout();
        return LayoutContext { ls.layout, ls.vbapTriplets, ls.ambiDecodeMatrix, ls.ambiNumSpeakers };
    }

    const SpeakerLayout& layout() const { return engine.getActiveLayout().layout; }
    int numSpeakers() const             { return layout().numSpeakers; }
};

std::vector<std::unique_ptr<Rig>> makeRigs (bool flat, bool height)
{
    std::vector<std::unique_ptr<Rig>> rigs;
    if (flat)
        for (const auto& f : kFlatFormats)
            rigs.push_back (std::make_unique<Rig> (f, false));
    if (height)
        for (const auto& f : kHeightFormats)
            rigs.push_back (std::make_unique<Rig> (f, true));
    return rigs;
}

void gainsAt (const SpatializationAlgorithm& algo, const LayoutContext& ctx, int n,
              float azDeg, float elDeg, float distance, float* out)
{
    for (int s = 0; s < MAX_SPEAKERS; ++s)
        out[s] = 0.0f;
    const SourcePosition src { juce::degreesToRadians (azDeg), juce::degreesToRadians (elDeg), distance };
    algo.computeGains (src, ctx, out, n);
}

float powerOf (const float* g, int n)
{
    float p = 0.0f;
    for (int s = 0; s < n; ++s)
        p += g[s] * g[s];
    return p;
}

bool allFinite (const float* g, int n)
{
    for (int s = 0; s < n; ++s)
        if (! std::isfinite (g[s]))
            return false;
    return true;
}

int argmax (const float* g, int n)
{
    int best = 0;
    for (int s = 1; s < n; ++s)
        if (g[s] > g[best])
            best = s;
    return best;
}

/** mirror[s] = the speaker at (-az, el) of speaker s. Every speaker of every
    shipped layout has one (0 and 180 degrees and the SML zenith map onto
    themselves); returns false if any does not. */
bool buildMirrorMap (const SpeakerLayout& layout, int* mirror)
{
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    for (int s = 0; s < layout.numSpeakers; ++s)
    {
        mirror[s] = -1;
        for (int t = 0; t < layout.numSpeakers; ++t)
        {
            const double azSum  = std::remainder (static_cast<double> (layout.speakers[s].azimuthRad)
                                                + static_cast<double> (layout.speakers[t].azimuthRad), twoPi);
            const double elDiff = static_cast<double> (layout.speakers[s].elevationRad)
                                - static_cast<double> (layout.speakers[t].elevationRad);
            if (std::abs (azSum) < 1e-4 && std::abs (elDiff) < 1e-4)
            {
                mirror[s] = t;
                break;
            }
        }
        if (mirror[s] < 0)
            return false;
    }
    return true;
}

/** Coplanar-tie filter (RESEARCH F5, AndrewRahman/SpatialCore#22): true when the
    minimum-sum enclosing regular triplet -- the one computeVBAPGains3D selects --
    has a rival enclosing triplet whose gain sum is within 1e-4 of it but whose
    gain vector differs by more than 1e-3. There the winner is decided by float
    rounding, so mirror symmetry is not a property of the code.

    Only rivals of the minimum count. The regular list holds every valid triple,
    and any two triangles drawn from one coplanar speaker set have identical gain
    sums (sum g = p.n / l.n for vertices l on a plane with normal n), so ties
    between non-minimal triplets are everywhere on 7.1.6, 9.1.6 and SML13.1 and
    never reach the output. */
bool coplanarTieAt (const LayoutContext& ctx, float azDeg, float elDeg)
{
    const float az = juce::degreesToRadians (azDeg);
    const float el = juce::degreesToRadians (elDeg);
    const float px = std::cos (el) * std::sin (az);
    const float py = std::cos (el) * std::cos (az);
    const float pz = std::sin (el);
    const int   n  = ctx.layout.numSpeakers;

    struct Candidate { float sum; float g[MAX_SPEAKERS]; };
    std::vector<Candidate> found;

    for (const auto& tri : ctx.triplets)
    {
        if (tri.lowerHemisphere)
            continue;
        const float g0 = tri.inv[0][0] * px + tri.inv[0][1] * py + tri.inv[0][2] * pz;
        const float g1 = tri.inv[1][0] * px + tri.inv[1][1] * py + tri.inv[1][2] * pz;
        const float g2 = tri.inv[2][0] * px + tri.inv[2][1] * py + tri.inv[2][2] * pz;
        if (g0 < -1e-6f || g1 < -1e-6f || g2 < -1e-6f)
            continue;

        Candidate c {};
        c.sum = g0 + g1 + g2;
        const float a = std::max (0.0f, g0), b = std::max (0.0f, g1), d = std::max (0.0f, g2);
        const float p = a * a + b * b + d * d;
        const float scale = p > 1e-12f ? 1.0f / std::sqrt (p) : 0.0f;
        c.g[tri.i] = a * scale;
        c.g[tri.j] = b * scale;
        c.g[tri.k] = d * scale;
        found.push_back (c);
    }

    if (found.empty())
        return false;

    size_t best = 0;
    for (size_t x = 1; x < found.size(); ++x)
        if (found[x].sum < found[best].sum)
            best = x;

    for (size_t y = 0; y < found.size(); ++y)
    {
        if (y == best || std::abs (found[y].sum - found[best].sum) >= 1e-4f)
            continue;
        float diff = 0.0f;
        for (int s = 0; s < n; ++s)
            diff = std::max (diff, std::abs (found[best].g[s] - found[y].g[s]));
        if (diff > 1e-3f)
            return true;
    }
    return false;
}

/** KNN ties: true when the 3rd- and 4th-nearest angular distances differ by
    less than 1e-4 rad, so which speaker joins the k = 3 set is arbitrary. */
bool knnTieAt (const SpeakerLayout& layout, float azDeg, float elDeg)
{
    const int n = layout.numSpeakers;
    if (n <= 3)
        return false;
    const float az = juce::degreesToRadians (azDeg);
    const float el = juce::degreesToRadians (elDeg);
    const float px = std::cos (el) * std::sin (az);
    const float py = std::cos (el) * std::cos (az);
    const float pz = std::sin (el);

    float d[MAX_SPEAKERS];
    for (int s = 0; s < n; ++s)
    {
        const auto& spk = layout.speakers[s];
        const float sx = std::cos (spk.elevationRad) * std::sin (spk.azimuthRad);
        const float sy = std::cos (spk.elevationRad) * std::cos (spk.azimuthRad);
        const float sz = std::sin (spk.elevationRad);
        d[s] = std::acos (juce::jlimit (-1.0f, 1.0f, px * sx + py * sy + pz * sz));
    }
    std::sort (d, d + n);
    return std::abs (d[3] - d[2]) < 1e-4f;
}

struct StepResult
{
    float maxStep = 0.0f;
    float atAzDeg = 0.0f;
    int   speaker = -1;
    int   nonFinite = 0;      // NaN/Inf steps, kept out of maxStep (WR-07)
};

/** Largest per-speaker gain change between consecutive azimuths of a full
    360-degree sweep at one elevation, including the step that closes the circle. */
StepResult maxStepOverSweep (const SpatializationAlgorithm& algo, const LayoutContext& ctx, int n,
                             float elDeg, float stepDeg, float distance)
{
    const int count = static_cast<int> (std::lround (360.0f / stepDeg));
    float first[MAX_SPEAKERS] = {}, prev[MAX_SPEAKERS] = {}, cur[MAX_SPEAKERS] = {};
    StepResult r;

    auto compare = [&] (const float* a, const float* b, float azDeg)
    {
        for (int s = 0; s < n; ++s)
        {
            const float before = r.maxStep;
            if (! spatialcore_test::accumulateWorstFinite (r.maxStep, std::abs (a[s] - b[s])))
            {
                ++r.nonFinite;
                continue;
            }
            if (r.maxStep > before)
            {
                r.atAzDeg = azDeg;
                r.speaker = s;
            }
        }
    };

    for (int k = 0; k < count; ++k)
    {
        const float azDeg = -180.0f + stepDeg * static_cast<float> (k);
        gainsAt (algo, ctx, n, azDeg, elDeg, distance, cur);
        if (k == 0)
            std::memcpy (first, cur, sizeof (cur));
        else
            compare (prev, cur, azDeg);
        std::memcpy (prev, cur, sizeof (cur));
    }
    compare (prev, first, 180.0f);
    return r;
}
} // namespace

// ============================================================================
// Law table completeness
// ============================================================================

TEST_CASE ("Panning laws: every algorithm in AllAlgorithmTypes has declared laws (D-13)", "[panning-law]")
{
    const auto algorithms = instantiateAll (AllAlgorithmTypes{});
    REQUIRE (static_cast<int> (algorithms.size()) == NUM_ALGORITHMS);

    for (const auto& algo : algorithms)
    {
        INFO ("algorithm " << algo->getName().toStdString());
        const PanningLaws* laws = lawsFor (*algo);
        REQUIRE (laws != nullptr);

        // A surround algorithm must declare real surround laws, and the one
        // binaural-only algorithm must not pretend to have them.
        if (algo->supportsSurround())
        {
            CHECK (laws->onSpeaker != OnSpeakerLaw::NotApplicable);
            CHECK (laws->mirror != MirrorRule::NotApplicable);
            CHECK (laws->powerTol > 0.0f);
        }
        else
        {
            CHECK (laws->onSpeaker == OnSpeakerLaw::NotApplicable);
            CHECK (algo->supportsBinauralDirect());
        }
    }
}

// ============================================================================
// (1) On-speaker behaviour
// ============================================================================

TEST_CASE ("Panning laws: on-speaker behaviour (D-13)", "[panning-law][unity]")
{
    const auto algorithms = instantiateAll (AllAlgorithmTypes{});
    const auto rigs = makeRigs (true, true);

    for (const auto& rig : rigs)
    {
        const LayoutContext ctx = rig->context();
        const SpeakerLayout& layout = rig->layout();
        const int n = rig->numSpeakers();

        for (const auto& algo : algorithms)
        {
            if (! algo->supportsSurround())
                continue;
            const PanningLaws* laws = lawsFor (*algo);
            REQUIRE (laws != nullptr);

            for (int s = 0; s < n; ++s)
            {
                // The speaker's own direction, so the float matches exactly. DBAP is
                // tested at distance 1 (on the speaker sphere), everything else at 0.5.
                const float distance = laws->onSpeaker == OnSpeakerLaw::NearUnity ? 1.0f : 0.5f;
                const SourcePosition src { layout.speakers[s].azimuthRad, layout.speakers[s].elevationRad, distance };
                float g[MAX_SPEAKERS] = {};
                algo->computeGains (src, ctx, g, n);

                INFO (rig->name << " " << laws->name << " speaker " << s);
                REQUIRE (allFinite (g, n));

                switch (laws->onSpeaker)
                {
                    case OnSpeakerLaw::Unity:
                    {
                        // KNN's own on-speaker shortcut compares acos (dot) with 1e-4, and a
                        // float dot of 0.99999994 already gives ~3.5e-4, so KNN reaches 1 via
                        // its weights rather than exactly -- hence a tolerance, not ==.
                        CHECK_THAT (g[s], WithinAbs (1.0f, 1e-5f));
                        const float othersTol = rig->height ? laws->onSpeakerOthersTolHeight : 1e-5f;
                        for (int t = 0; t < n; ++t)
                            if (t != s)
                                CHECK_THAT (g[t], WithinAbs (0.0f, othersTol));
                        break;
                    }
                    case OnSpeakerLaw::NearUnity:
                        // d^2 is clamped at 0.001, so DBAP is unity only to ~1e-3.
                        CHECK (g[s] >= 1.0f - 2e-3f);
                        break;
                    case OnSpeakerLaw::ArgmaxBelowOne:
                        // MDAP spreads by design (F3): the speaker leads but is not unity.
                        CHECK (argmax (g, n) == s);
                        CHECK (g[s] < 1.0f - 1e-3f);
                        break;
                    case OnSpeakerLaw::Argmax:
                        CHECK (argmax (g, n) == s);
                        break;
                    case OnSpeakerLaw::FiniteOnly:
                        // A mode-matching Ambisonics decode has no on-speaker law: the
                        // decoded pattern spreads over every speaker the order reaches.
                        break;
                    case OnSpeakerLaw::NotApplicable:
                        FAIL ("surround algorithm with no on-speaker law");
                        break;
                }
            }
        }
    }
}

// ============================================================================
// (2) Unit power across sweeps
// ============================================================================

TEST_CASE ("Panning laws: unit power across sweeps (D-13)", "[panning-law][power]")
{
    const auto algorithms = instantiateAll (AllAlgorithmTypes{});
    const auto rigs = makeRigs (true, true);
    const float flatElevations[]   = { 0.0f };
    const float heightElevations[] = { -60.0f, -20.0f, 0.0f, 20.0f, 45.0f, 80.0f };

    for (const auto& rig : rigs)
    {
        const LayoutContext ctx = rig->context();
        const int n = rig->numSpeakers();
        const float* elevations = rig->height ? heightElevations : flatElevations;
        const int numElevations = rig->height ? 6 : 1;

        for (const auto& algo : algorithms)
        {
            if (! algo->supportsSurround())
                continue;
            const PanningLaws* laws = lawsFor (*algo);
            REQUIRE (laws != nullptr);

            int failures = 0;
            float worstErr = 0.0f, worstAz = 0.0f, worstEl = 0.0f;

            for (int e = 0; e < numElevations; ++e)
                for (int k = 0; k < 720; ++k)
                {
                    const float azDeg = -179.63f + 0.5f * static_cast<float> (k);
                    float g[MAX_SPEAKERS] = {};
                    gainsAt (*algo, ctx, n, azDeg, elevations[e], 0.5f, g);
                    const float p = powerOf (g, n);
                    float err = std::isfinite (p) ? std::abs (p - 1.0f) : 1e30f;

                    // Ambisonics clamps negative speaker gains, so a below-horizon
                    // source on a layout with no lower speakers may legitimately
                    // decode to nothing: off the flat horizon, exact silence is lawful.
                    const bool flatHorizon = ! rig->height && elevations[e] == 0.0f;
                    if (laws->mayBeSilentOffFlatHorizon && ! flatHorizon && p == 0.0f)
                        err = 0.0f;

                    if (err > laws->powerTol)
                        ++failures;
                    if (err > worstErr)
                    {
                        worstErr = err;
                        worstAz  = azDeg;
                        worstEl  = elevations[e];
                    }
                }

            INFO (rig->name << " " << laws->name << " worst |power - 1| " << worstErr
                  << " at az " << worstAz << " el " << worstEl);
            CHECK (failures == 0);
        }
    }
}

// ============================================================================
// (3) Left/right mirror symmetry
// ============================================================================

TEST_CASE ("Panning laws: left/right mirror symmetry (D-13)", "[panning-law][mirror]")
{
    // Every shipped layout is left/right symmetric, so az <-> -az must permute the
    // gains by the layout's mirror map. On height layouts VBAP/VBIP are checked only
    // off the coplanar-quad ties, where the min-sum triplet winner is decided by
    // float rounding (RESEARCH F5, AndrewRahman/SpatialCore#22); KNN only where the
    // 3rd/4th nearest speakers do not tie (k-nearest ties break symmetry by
    // construction); MDAP on flat layouts only.
    const auto algorithms = instantiateAll (AllAlgorithmTypes{});
    const auto rigs = makeRigs (true, true);
    const float flatElevations[]   = { 0.0f };
    const float heightElevations[] = { 20.0f, 45.0f };

    for (const auto& rig : rigs)
    {
        const LayoutContext ctx = rig->context();
        const SpeakerLayout& layout = rig->layout();
        const int n = rig->numSpeakers();

        int mirror[MAX_SPEAKERS] = {};
        INFO (rig->name);
        REQUIRE (buildMirrorMap (layout, mirror));

        const float* elevations = rig->height ? heightElevations : flatElevations;
        const int numElevations = rig->height ? 2 : 1;

        for (const auto& algo : algorithms)
        {
            if (! algo->supportsSurround())
                continue;
            const PanningLaws* laws = lawsFor (*algo);
            REQUIRE (laws != nullptr);

            if (laws->mirror == MirrorRule::FlatOnly && rig->height)
                continue;

            const float tol = rig->height ? laws->mirrorTolHeight : 1e-4f;
            int checked = 0, skipped = 0, failures = 0;
            float worst = 0.0f, worstAz = 0.0f, worstEl = 0.0f;

            for (int e = 0; e < numElevations; ++e)
                for (int k = 0; k < 180; ++k)
                {
                    const float az = 0.37f + static_cast<float> (k);
                    const float el = elevations[e];

                    if (laws->mirror == MirrorRule::TieFilteredOnHeight && rig->height
                        && (coplanarTieAt (ctx, az, el) || coplanarTieAt (ctx, -az, el)))
                    {
                        ++skipped;
                        continue;
                    }
                    if (laws->mirror == MirrorRule::KnnTieFiltered
                        && (knnTieAt (layout, az, el) || knnTieAt (layout, -az, el)))
                    {
                        ++skipped;
                        continue;
                    }

                    float a[MAX_SPEAKERS] = {}, b[MAX_SPEAKERS] = {};
                    gainsAt (*algo, ctx, n, az, el, 0.5f, a);
                    gainsAt (*algo, ctx, n, -az, el, 0.5f, b);
                    ++checked;

                    float diff = 0.0f;
                    for (int s = 0; s < n; ++s)
                        diff = std::max (diff, std::abs (a[s] - b[mirror[s]]));
                    if (! (diff <= tol))
                        ++failures;
                    if (diff > worst)
                    {
                        worst   = diff;
                        worstAz = az;
                        worstEl = el;
                    }
                }

            INFO (laws->name << " checked " << checked << " skipped " << skipped
                  << " worst " << worst << " at az " << worstAz << " el " << worstEl);
            CHECK (failures == 0);
            CHECK (checked > 0);
        }
    }
}

// ============================================================================
// (4) 360-degree continuity, scoped to where the algorithm is continuous (D-18)
// ============================================================================

TEST_CASE ("Panning laws: 360-degree continuity where the algorithm is continuous (D-13, D-18)",
           "[panning-law][continuity]")
{
    // Excluded regions (D-18), not asserted here:
    //   - VBAP, VBIP and MDAP on height layouts off the horizon. The all-triples
    //     min-sum triplet selection has exact coplanar-quad ties whose winner is
    //     decided by float rounding, giving 0.45-0.85 jumps on 0.1-degree sweeps
    //     (RESEARCH F5). Tracked in AndrewRahman/SpatialCore#22.
    //   - KNN anywhere: k-nearest switching is inherent (0.03-0.54 per step).
    //
    // VBIP's bound is 0.08, not VBAP's 0.01, because the textbook law is square-root
    // shaped at a speaker: a step of d degrees off a speaker whose neighbour is
    // 30 degrees away moves the VBAP gain of the neighbour by about sin(d)/sin(30),
    // and VBIP's gain is the square root of that share -- sqrt(sin(0.1 deg) / sin(30 deg))
    // = sqrt(0.001745 / 0.5) = sqrt(0.00349) ~ 0.059 on the first 0.1-degree step
    // (measured 0.0591 on 9.1 and 9.1.6).
    //
    // MDAP on height layouts at the horizon is swept at 0.5-degree steps with a
    // bound of 0.12 (0.16-0.19 before D-04 gave its below-horizon ring points real
    // triplets).
    const auto algorithms = instantiateAll (AllAlgorithmTypes{});
    const auto rigs = makeRigs (true, true);
    const float horizonOnly[] = { 0.0f };
    const float everywhere[]  = { 0.0f, 20.0f, 40.0f };

    for (const auto& rig : rigs)
    {
        const LayoutContext ctx = rig->context();
        const int n = rig->numSpeakers();

        for (const auto& algo : algorithms)
        {
            const PanningLaws* laws = lawsFor (*algo);
            REQUIRE (laws != nullptr);
            if (! algo->supportsSurround() || laws->continuity == ContinuityScope::None)
                continue;

            const bool all = laws->continuity == ContinuityScope::Everywhere;
            const float* elevations = all ? everywhere : horizonOnly;
            const int numElevations = all ? 3 : 1;

            const float bound   = (rig->height && ! all) ? laws->continuityBoundHeight : laws->continuityBoundFlat;
            const float stepDeg = (rig->height && ! all) ? laws->heightStepDeg : 0.1f;

            for (int e = 0; e < numElevations; ++e)
            {
                const StepResult r = maxStepOverSweep (*algo, ctx, n, elevations[e], stepDeg, 0.5f);
                INFO (rig->name << " " << laws->name << " el " << elevations[e] << " step " << stepDeg
                      << " max step " << r.maxStep << " at az " << r.atAzDeg << " speaker " << r.speaker
                      << " non-finite steps " << r.nonFinite);
                CHECK (r.nonFinite == 0);
                CHECK (r.maxStep <= bound);
            }
        }
    }
}

// ============================================================================
// (5) DirectBinaural property checks
// ============================================================================

TEST_CASE ("DirectBinaural: property checks at the horizon (Discretion)", "[panning-law][directbinaural]")
{
    // Property checks only: left/right mirror, ILD and ITD sign, monotonic
    // magnitude versus |azimuth| at the horizon, and the distance gain law. No
    // elevation or front/back assertion -- BUG-01 changes those in Phase 3
    // (RESEARCH Pitfall 6).
    DirectBinauralAlgorithm algo;
    const BinauralContext bctx { 1, 48000.0, kDefaultBinauralProfiles };

    auto at = [&] (float azDeg, float distance)
    {
        return algo.computeBinauralGains ({ juce::degreesToRadians (azDeg), 0.0f, distance }, bctx);
    };

    SECTION ("mirroring azimuth swaps left and right")
    {
        for (int k = 0; k <= 180; k += 5)
        {
            const float az = static_cast<float> (k) + (k == 0 || k == 180 ? 0.0f : 0.37f);
            const BinauralGains a = at (az, 0.5f);
            const BinauralGains b = at (-az, 0.5f);
            INFO ("az " << az);
            CHECK_THAT (a.leftGain,          WithinAbs (b.rightGain,         1e-6f));
            CHECK_THAT (a.rightGain,         WithinAbs (b.leftGain,          1e-6f));
            CHECK_THAT (a.leftDelaySamples,  WithinAbs (b.rightDelaySamples, 1e-6f));
            CHECK_THAT (a.rightDelaySamples, WithinAbs (b.leftDelaySamples,  1e-6f));
        }
    }

    SECTION ("ILD and ITD point at the near ear")
    {
        for (int k = 5; k <= 175; k += 5)
        {
            const float az = static_cast<float> (k);
            INFO ("az " << az);

            const BinauralGains left = at (az, 0.5f);   // positive azimuth = left
            CHECK (left.rightGain < left.leftGain);
            CHECK (left.rightDelaySamples > 0.0f);
            CHECK (left.leftDelaySamples == 0.0f);

            const BinauralGains right = at (-az, 0.5f);
            CHECK (right.leftGain < right.rightGain);
            CHECK (right.leftDelaySamples > 0.0f);
            CHECK (right.rightDelaySamples == 0.0f);
        }
    }

    SECTION ("far-ear delay grows and far-ear gain falls with |azimuth| up to 90 degrees")
    {
        BinauralGains prev = at (0.0f, 0.5f);
        for (int k = 5; k <= 90; k += 5)
        {
            const BinauralGains cur = at (static_cast<float> (k), 0.5f);   // far ear = right
            INFO ("az " << k);
            CHECK (cur.rightDelaySamples >= prev.rightDelaySamples);
            CHECK (cur.rightGain <= prev.rightGain);
            prev = cur;
        }
    }

    SECTION ("distance gain law at azimuth 0")
    {
        const float distances[] = { 0.0f, 0.1f, 0.5f, 1.0f };
        for (float d : distances)
        {
            const BinauralGains g = at (0.0f, d);
            const float expected = 1.0f / std::max (0.1f, 4.0f * d + 0.25f);
            INFO ("distance " << d);
            CHECK_THAT (g.leftGain,  WithinAbs (expected, 1e-6f));
            CHECK_THAT (g.rightGain, WithinAbs (expected, 1e-6f));
        }
    }
}

// ============================================================================
// Textbook values (D-13, D-16): the generated PanningReference.h holds values
// generated from the published formulas, independent of the code under test.
// ============================================================================

namespace
{
template <size_t N>
void checkGains (const float* got, const float (&expected)[N], float tol)
{
    for (size_t s = 0; s < N; ++s)
    {
        INFO ("speaker " << s);
        CHECK_THAT (got[s], WithinAbs (expected[s], tol));
    }
}

/** A flat layoutDefs layout with an empty triplet vector (the 2D pair path). */
struct FlatDefContext
{
    const SpeakerLayout&     layout;
    std::vector<VBAPTriplet> triplets;
    float                    ambi[1][MAX_SPEAKERS] = {};

    explicit FlatDefContext (LayoutID id) : layout (getLayoutDef (id)) {}
    LayoutContext context() const { return LayoutContext { layout, triplets, ambi, 0 }; }
    int numSpeakers() const       { return layout.numSpeakers; }
};
} // namespace

TEST_CASE ("Panning laws: textbook values from the published formulas (D-13, D-16)", "[panning-law][textbook]")
{
    using namespace spatialcore_ref;
    VBAPAlgorithm vbap;
    VBIPAlgorithm vbip;
    DBAPAlgorithm dbap;

    const FlatDefContext quad (Quad);
    const FlatDefContext s50 (S5_0);
    float g[MAX_SPEAKERS] = {};

    // VBAP (Pulkki 1997) and VBIP (Pernaux, Boussard & Jot, DAFx-98 sec. 2.2.2).
    gainsAt (vbap, quad.context(), quad.numSpeakers(), 30.0f, 0.0f, 0.5f, g);
    checkGains (g, kVbap_Quad_az30, 1e-5f);
    gainsAt (vbip, quad.context(), quad.numSpeakers(), 30.0f, 0.0f, 0.5f, g);
    checkGains (g, kVbip_Quad_az30, 1e-5f);

    gainsAt (vbap, s50.context(), s50.numSpeakers(), 10.0f, 0.0f, 0.5f, g);
    checkGains (g, kVbap_S5_0_az10, 1e-5f);
    gainsAt (vbip, s50.context(), s50.numSpeakers(), 10.0f, 0.0f, 0.5f, g);
    checkGains (g, kVbip_S5_0_az10, 1e-5f);

    gainsAt (vbap, s50.context(), s50.numSpeakers(), 50.0f, 0.0f, 0.5f, g);
    checkGains (g, kVbap_S5_0_az50, 1e-5f);
    gainsAt (vbip, s50.context(), s50.numSpeakers(), 50.0f, 0.0f, 0.5f, g);
    checkGains (g, kVbip_S5_0_az50, 1e-5f);

    // DBAP: Lossius et al., ICMC 2009 eq. 2-5 at SpatialCore's effective
    // R = 12.04 dB (a = 2), in code speaker order 45, -45, 135, -135 (F2).
    gainsAt (dbap, quad.context(), quad.numSpeakers(), 30.0f, 0.0f, 1.0f, g);
    checkGains (g, kDbap_Quad_az30_dist1, 1e-5f);
    gainsAt (dbap, quad.context(), quad.numSpeakers(), 30.0f, 0.0f, 0.5f, g);
    checkGains (g, kDbap_Quad_az30_dist05, 1e-5f);

    // 3D VBAP on the engine-built 7.1.4 context. The generator proved each pin
    // lies in a unique minimum-sum triplet, so no coplanar tie (F5, #22) applies.
    const Rig s714 ({ OutputFormat::Surround7_1_4, "7.1.4" }, true);
    REQUIRE (s714.numSpeakers() == 11);
    for (const auto& pin : kVbap3D_S7_1_4)
    {
        INFO ("7.1.4 az " << pin.azimuthDeg << " el " << pin.elevationDeg);
        gainsAt (vbap, s714.context(), s714.numSpeakers(), pin.azimuthDeg, pin.elevationDeg, 0.5f, g);
        checkGains (g, pin.gains, 1e-5f);
    }
}

TEST_CASE ("Panning laws: MDAP matches a port of its ring construction (cross-check, not an oracle)",
           "[panning-law][textbook]")
{
    // There is no spread parameter -- the ring angle is clamp(0.9 * 180 / N, 5, 30)
    // degrees and never 0 -- so "MDAP at spread 0 equals VBAP" cannot be tested
    // (RESEARCH F3). kMdapPort_* come from a numpy port of the same ring
    // construction (main direction + 8 aux points, summed 2D VBAP, L2-normalised):
    // this checks the ring geometry, not an independent panning law.
    using namespace spatialcore_ref;
    MDAPAlgorithm mdap;
    const FlatDefContext quad (Quad);
    const FlatDefContext s50 (S5_0);
    float g[MAX_SPEAKERS] = {};

    gainsAt (mdap, quad.context(), quad.numSpeakers(), 30.0f, 0.0f, 0.5f, g);
    checkGains (g, kMdapPort_Quad_az30, 5e-4f);
    gainsAt (mdap, s50.context(), s50.numSpeakers(), 10.0f, 0.0f, 0.5f, g);
    checkGains (g, kMdapPort_S5_0_az10, 5e-4f);
}

// ============================================================================
// Height layouts, including below the horizon (D-13, D-04)
// ============================================================================

TEST_CASE ("Panning laws: VBAP, VBIP and MDAP cover every height layout including below the horizon (D-13, D-04)",
           "[panning-law][height]")
{
    // Every gain finite and at unit power over the whole sphere, and below the
    // horizon no elevated speaker (el > 10 degrees) receives gain: at or below
    // -1 degree for VBAP and VBIP (the EAR lower hemisphere uses ear-level
    // speakers, their -30 copies and a nadir only), at or below -45 for MDAP,
    // whose ring reaches up to about 23 degrees above the source on these layouts.
    const VBAPAlgorithm vbap;
    const VBIPAlgorithm vbip;
    const MDAPAlgorithm mdap;
    struct Case { const SpatializationAlgorithm* algo; float powerTol; float silentAtOrBelowDeg; };
    const Case cases[] = { { &vbap, 1e-5f, -1.0f }, { &vbip, 1e-5f, -1.0f }, { &mdap, 1e-4f, -45.0f } };

    std::vector<float> elevations;
    elevations.push_back (-90.0f);
    for (int e = -89; e <= 89; e += 2)
        elevations.push_back (static_cast<float> (e));
    elevations.push_back (90.0f);

    const auto rigs = makeRigs (false, true);
    for (const auto& rig : rigs)
    {
        const LayoutContext ctx = rig->context();
        const SpeakerLayout& layout = rig->layout();
        const int n = rig->numSpeakers();
        const float elevatedRad = juce::degreesToRadians (10.0f);

        for (const auto& c : cases)
        {
            int nonFinite = 0, offPower = 0, leaks = 0;
            float worstPowerErr = 0.0f;

            for (float el : elevations)
                for (int k = 0; k < 180; ++k)
                {
                    const float az = 0.5f + 2.0f * static_cast<float> (k);
                    float g[MAX_SPEAKERS] = {};
                    gainsAt (*c.algo, ctx, n, az, el, 0.5f, g);

                    if (! allFinite (g, n))
                    {
                        ++nonFinite;
                        continue;
                    }
                    const float err = std::abs (powerOf (g, n) - 1.0f);
                    worstPowerErr = std::max (worstPowerErr, err);
                    if (err > c.powerTol)
                        ++offPower;

                    if (el <= c.silentAtOrBelowDeg)
                        for (int s = 0; s < n; ++s)
                            if (layout.speakers[s].elevationRad > elevatedRad && g[s] != 0.0f)
                                ++leaks;
                }

            INFO (rig->name << " " << c.algo->getName().toStdString() << " worst |power - 1| " << worstPowerErr);
            CHECK (nonFinite == 0);
            CHECK (offPower == 0);
            CHECK (leaks == 0);
        }
    }
}

TEST_CASE ("Panning laws: VBAP lower-hemisphere continuity, scoped (D-04, D-18)", "[panning-law][continuity]")
{
    // Nadir cap: below the -30 ring the EAR construction is a fan of triangles
    // around the virtual nadir, and VBAP is continuous there (at most 0.01 per
    // 0.1 degree). 5.1.2 and 5.1.4 are checked only from -65 down: their sparse
    // rear ring (110 / -110, a 140-degree gap) is a pair wedge down to about -59
    // degrees before the cap takes over.
    //
    // The -30..0 band is one pair-pan region per neighbouring ear-level pair
    // (G-02-2): the horizon pan at the source azimuth, continuous at the
    // horizon slope, so it gets the same 0.01 bound (it needed a far looser
    // one while the band was coplanar quads split into tied triangles,
    // RESEARCH F5). The seam between band and cap (-35, -40) is checked on
    // every rig: on 5.1.x and 7.1.x those sweeps cross between a pair wedge
    // and a cap triangle, which agree analytically on their shared edge (on
    // the cap's top face the nadir share is 0 and both reduce to the same pair
    // pan). A seam or band sweep above 0.01 is a construction defect, not a
    // bound to loosen.
    const VBAPAlgorithm vbap;

    struct Scope { const char* label; std::vector<float> elevations; float bound; };
    const auto rigs = makeRigs (false, true);

    for (const auto& rig : rigs)
    {
        const LayoutContext ctx = rig->context();
        const int n = rig->numSpeakers();
        const bool sparseRear = std::strcmp (rig->name, "5.1.2") == 0 || std::strcmp (rig->name, "5.1.4") == 0;

        const Scope scopes[] = {
            { "nadir cap", sparseRear ? std::vector<float> { -65.0f, -75.0f }
                                      : std::vector<float> { -50.0f, -60.0f, -75.0f }, 0.01f },
            { "-30..0 band", { -5.0f, -15.0f, -25.0f }, 0.01f },
            { "band/cap seam", { -35.0f, -40.0f }, 0.01f },
        };

        for (const auto& scope : scopes)
            for (float el : scope.elevations)
            {
                const StepResult r = maxStepOverSweep (vbap, ctx, n, el, 0.1f, 0.5f);
                INFO (rig->name << " " << scope.label << " el " << el << " max step " << r.maxStep
                      << " at az " << r.atAzDeg << " speaker " << r.speaker
                      << " non-finite steps " << r.nonFinite);
                CHECK (r.nonFinite == 0);
                CHECK (r.maxStep <= scope.bound);
            }
    }
}
