#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// Shape indices (alphabetical, matching computeTrajectory's switch — mirrors
// Tests/TrajectoryTests.cpp's TrajShape namespace in the OSD monolith).
namespace TrajShape
{
    constexpr int None = 0, Bounce = 1, Circle = 2, Cross = 3, Figure8 = 4,
                  Heart = 5, Helix = 6, Infinity = 7, Line = 8, Orbit = 9,
                  Random = 10, Spiral = 11, Square = 12, Triangle = 13;
}

// ============================================================================
// Characterization tests — deeper than the 08-02 smoke test, all 13 shapes
// plus None, replicating Tests/TrajectoryTests.cpp's style but calling
// spatialcore::TrajectoryEngine:: directly (D-09 fresh-test-separate-commit).
// ============================================================================

TEST_CASE ("wrapAzimuth returns for any input and keeps in-range results (WR-05)", "[trajectory][edge]")
{
    // Unchanged for the values the engine itself produces.
    CHECK (wrapAzimuth (0.0f) == 0.0f);
    CHECK (wrapAzimuth (180.0f) == 180.0f);
    CHECK (wrapAzimuth (-180.0f) == -180.0f);
    CHECK (wrapAzimuth (181.0f) == -179.0f);
    CHECK (wrapAzimuth (-181.0f) == 179.0f);
    CHECK (wrapAzimuth (540.0f) == 180.0f);

    // Far from the range: these used to loop forever (1e10 - 360 == 1e10 in float).
    for (float az : { 1.0e4f, -1.0e4f, 1.0e10f, -1.0e10f, 1.0e30f, -3.4e38f })
    {
        CAPTURE (az);
        const float w = wrapAzimuth (az);
        CHECK (std::isfinite (w));
        CHECK (w >= -180.0f);
        CHECK (w <= 180.0f);
    }

    // Non-finite input returns (it used to hang on infinity) and stays non-finite.
    CHECK (std::isnan (wrapAzimuth (std::numeric_limits<float>::quiet_NaN())));
    CHECK (std::isnan (wrapAzimuth (std::numeric_limits<float>::infinity())));
    CHECK (std::isnan (wrapAzimuth (-std::numeric_limits<float>::infinity())));
}

TEST_CASE ("None shape returns base position unchanged", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::None, 0.5f, 45.0f, 20.0f, 0.7f);
    CHECK_THAT (r.azDeg, WithinAbs (45.0f, 0.01f));
    CHECK_THAT (r.elDeg, WithinAbs (20.0f, 0.01f));
    CHECK_THAT (r.dist,  WithinAbs (0.7f, 0.001f));
    CHECK_FALSE (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK_FALSE (r.controlsDist);
}

TEST_CASE ("Orbit at phase 0 starts at base azimuth", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.0f, 90.0f, 10.0f, 0.5f);
    CHECK_THAT (r.azDeg, WithinAbs (90.0f, 0.01f));
    CHECK_THAT (r.elDeg, WithinAbs (10.0f, 0.01f));
    CHECK_THAT (r.dist,  WithinAbs (0.5f, 0.001f));
    CHECK (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK_FALSE (r.controlsDist);
}

TEST_CASE ("Orbit at phase 0.5 is 180 degrees from base", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.5f, 0.0f, 0.0f, 0.5f);
    CHECK (std::abs (std::abs (r.azDeg) - 180.0f) < 0.01f);
}

TEST_CASE ("Orbit at phase 0.25 is +90 degrees from base", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.25f, 0.0f, 0.0f, 0.5f);
    CHECK_THAT (r.azDeg, WithinAbs (90.0f, 0.01f));
}

TEST_CASE ("Bounce controlsAz and controlsEl flags", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Bounce, 0.5f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK_FALSE (r.controlsDist);
}

TEST_CASE ("Bounce at phase 0.5 is at peak displacement", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Bounce, 0.5f, 0.0f, 0.0f, 0.5f);
    CHECK_THAT (r.azDeg, WithinAbs (-90.0f, 0.01f));
    CHECK_THAT (r.elDeg, WithinAbs (-30.0f, 0.01f));
}

TEST_CASE ("Circle controls azimuth and distance", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Circle, 0.3f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Cross controls azimuth and elevation", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Cross, 0.1f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK_FALSE (r.controlsDist);
}

TEST_CASE ("Figure8 controls all three axes", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Figure8, 0.2f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Heart controls azimuth and distance", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Heart, 0.1f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Helix at phase 0 starts at baseAz, elevation +90", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Helix, 0.0f, 45.0f, 0.0f, 0.5f);
    CHECK_THAT (r.azDeg, WithinAbs (45.0f, 0.01f));
    CHECK_THAT (r.elDeg, WithinAbs (90.0f, 0.5f));
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK_FALSE (r.controlsDist);
}

TEST_CASE ("Helix at phase 1.0 reaches elevation -90", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Helix, 0.999f, 0.0f, 0.0f, 0.5f);
    CHECK (r.elDeg < -85.0f);
}

TEST_CASE ("Infinity controls azimuth and distance", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Infinity, 0.25f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Infinity is origin-relative (lemniscate)", "[trajectory][characterization]")
{
    auto r1 = TrajectoryEngine::computeTrajectory (TrajShape::Infinity, 0.0f, 0.0f, 0.0f, 0.3f);
    auto r2 = TrajectoryEngine::computeTrajectory (TrajShape::Infinity, 0.0f, 0.0f, 0.0f, 0.7f);
    CHECK (r1.dist > 0.5f);
    CHECK (r2.dist > 0.5f);
    CHECK (r1.dist <= 1.0f);
    CHECK (r2.dist <= 1.0f);
}

TEST_CASE ("Line controls azimuth and distance", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Line, 0.0f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Random controls all three axes", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Random, 0.3f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Spiral controls azimuth and distance", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Spiral, 0.3f, 0.0f, 0.0f, 0.0f);
    CHECK (r.controlsAz);
    CHECK_FALSE (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Square controls all three axes", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Square, 0.1f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK (r.controlsDist);
}

TEST_CASE ("Triangle controls all three axes", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Triangle, 0.1f, 0.0f, 0.0f, 0.5f);
    CHECK (r.controlsAz);
    CHECK (r.controlsEl);
    CHECK (r.controlsDist);
}

// --- Azimuth wrapping ---
TEST_CASE ("Azimuth wraps to -180..+180 range", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.75f, 90.0f, 0.0f, 0.5f);
    CHECK (r.azDeg >= -180.0f);
    CHECK (r.azDeg <= 180.0f);
}

// --- Distance clamping ---
TEST_CASE ("Distance clamped to 0..1", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Infinity, 0.125f, 0.0f, 0.0f, 0.95f);
    CHECK (r.dist >= 0.0f);
    CHECK (r.dist <= 1.0f);
}

// --- Elevation clamping ---
TEST_CASE ("Elevation clamped to -90..+90", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (TrajShape::Bounce, 0.5f, 0.0f, 80.0f, 0.5f);
    CHECK (r.elDeg >= -90.0f);
    CHECK (r.elDeg <= 90.0f);
}

// --- Reverse mode ---
TEST_CASE ("Reverse flips phase", "[trajectory][characterization]")
{
    auto fwd = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.25f, 0.0f, 0.0f, 0.5f);
    auto rev = TrajectoryEngine::computeTrajectory (TrajShape::Orbit, 0.75f, 0.0f, 0.0f, 0.5f, true);
    CHECK_THAT (fwd.azDeg, WithinAbs (rev.azDeg, 0.01f));
}

// --- controlsAz/El/Dist flags for all shapes, in one sweep (mirrors OSD's
//     "Control flags are correct for all shapes" characterization test) ---
TEST_CASE ("Control flags are correct for all shapes", "[trajectory][characterization]")
{
    struct Expected { int shape; bool az, el, dist; };
    std::vector<Expected> cases = {
        { TrajShape::None,     false, false, false },
        { TrajShape::Bounce,   true,  true,  false },
        { TrajShape::Circle,   true,  false, true  },
        { TrajShape::Cross,    true,  true,  false },
        { TrajShape::Figure8,  true,  true,  true  },
        { TrajShape::Heart,    true,  false, true  },
        { TrajShape::Helix,    true,  true,  false },
        { TrajShape::Infinity, true,  false, true  },
        { TrajShape::Line,     true,  false, true  },
        { TrajShape::Orbit,    true,  false, false },
        { TrajShape::Random,   true,  true,  true  },
        { TrajShape::Spiral,   true,  false, true  },
        { TrajShape::Square,   true,  true,  true  },
        { TrajShape::Triangle, true,  true,  true  },
    };

    for (auto& c : cases)
    {
        auto r = TrajectoryEngine::computeTrajectory (c.shape, 0.3f, 30.0f, 15.0f, 0.5f);
        INFO ("Shape " << c.shape);
        CHECK (r.controlsAz   == c.az);
        CHECK (r.controlsEl   == c.el);
        CHECK (r.controlsDist == c.dist);
    }
}

// --- Distance-scaling collapse-at-1.0 spot checks (edge phases) ---
TEST_CASE ("Shapes with no distance modulation are unaffected", "[trajectory][distance-scaling]")
{
    for (int shape : { TrajShape::Bounce, TrajShape::Cross, TrajShape::Helix, TrajShape::Orbit })
    {
        auto r = TrajectoryEngine::computeTrajectory (shape, 0.5f, 45.0f, 20.0f, 0.7f);
        INFO ("Shape " << shape);
        CHECK_THAT (r.dist, WithinAbs (0.7f, 0.01f));
    }
}

// --- tick()/ObjectInput integration: shape-change resets phase, None deactivates ---
TEST_CASE ("tick(): None shape deactivates the object", "[trajectory][tick]")
{
    TrajectoryEngine engine;
    TrajectoryEngine::ObjectInput input;
    input.shape = TrajShape::None;
    engine.tick (0, input);
    CHECK_FALSE (engine.isActive (0));
}

TEST_CASE ("tick(): active shape sets isActive true and advances phase", "[trajectory][tick]")
{
    TrajectoryEngine engine;
    TrajectoryEngine::ObjectInput input;
    input.shape = TrajShape::Orbit;
    input.speed = 1.0f;
    input.originAz = 0.0f;
    input.originEl = 0.0f;
    input.originDist = 0.5f;

    engine.tick (0, input, 0.1f);
    CHECK (engine.isActive (0));
    // After one tick at speed 1.0 for dt=0.1, phase=0.1 -> az = 0 + 360*0.1 = 36
    CHECK_THAT (engine.getFinalAz (0), WithinAbs (36.0f, 0.5f));
}

TEST_CASE ("tick(): oscOverride freezes the object (no tick applied)", "[trajectory][tick]")
{
    TrajectoryEngine engine;
    TrajectoryEngine::ObjectInput input;
    input.shape = TrajShape::Orbit;
    input.speed = 1.0f;
    input.oscOverride = true;

    engine.tick (0, input, 0.5f);
    // oscOverride returns early before setting active_ true
    CHECK_FALSE (engine.isActive (0));
}

TEST_CASE ("resetAll(): clears all objects to inactive with default position", "[trajectory][reset]")
{
    TrajectoryEngine engine;
    TrajectoryEngine::ObjectInput input;
    input.shape = TrajShape::Orbit;
    engine.tick (0, input, 0.2f);
    CHECK (engine.isActive (0));

    engine.resetAll();
    CHECK_FALSE (engine.isActive (0));
    CHECK_THAT (engine.getFinalDist (0), WithinAbs (0.5f, 0.001f));
}

// ============================================================================
// Reverse for all 13 shapes (Phase 4, EXTR-04, ROADMAP criterion 3; D-13 / D-19).
//
// Ten shapes retrace the forward path when reversed. Bounce and Line keep their deliberate
// OSD#100 reverse and are pinned here as exceptions to that rule (D-19): changing either would
// change what OpenSpatialDelay users hear. Random is checked only for movement and range.
// ============================================================================

namespace
{
    // Shortest difference between two angles, in degrees, always >= 0.
    float angularDiff (float a, float b)
    {
        float d = std::fmod (a - b, 360.0f);
        if (d > 180.0f)  d -= 360.0f;
        if (d < -180.0f) d += 360.0f;
        return std::abs (d);
    }

    struct Base { float az, el, dist; };

    bool inRange (float az, float el, float dist)
    {
        return std::isfinite (az) && std::isfinite (el) && std::isfinite (dist)
            && az >= -180.0f && az <= 180.0f
            && el >= -90.0f  && el <= 90.0f
            && dist >= 0.0f  && dist <= 1.0f;
    }
}

TEST_CASE ("Trajectory reverse: ten shapes retrace the forward path", "[trajectory][reverse]")
{
    const int shapes[] = { TrajShape::Circle, TrajShape::Cross, TrajShape::Figure8, TrajShape::Heart,
                           TrajShape::Helix, TrajShape::Infinity, TrajShape::Orbit, TrajShape::Spiral,
                           TrajShape::Square, TrajShape::Triangle };
    const Base bases[] = { { 30.0f, 10.0f, 0.5f }, { -120.0f, -20.0f, 0.8f }, { 0.0f, 0.0f, 0.2f } };

    for (int shape : shapes)
    {
        float maxErr = 0.0f;
        for (const auto& base : bases)
        {
            for (int k = 0; k <= 100; ++k)
            {
                const float p = (float) k / 100.0f;
                const auto fwd = TrajectoryEngine::computeTrajectory (shape, 1.0f - p, base.az, base.el, base.dist);
                const auto rev = TrajectoryEngine::computeTrajectory (shape, p, base.az, base.el, base.dist, true);

                const float eAz = angularDiff (rev.azDeg, fwd.azDeg);
                const float eEl = std::abs (rev.elDeg - fwd.elDeg);
                const float eDist = std::abs (rev.dist - fwd.dist);
                maxErr = std::max ({ maxErr, eAz, eEl, eDist });

                CAPTURE (shape, base.az, base.el, base.dist, p);
                CHECK_THAT (eAz, WithinAbs (0.0f, 1e-4f));
                CHECK_THAT (eEl, WithinAbs (0.0f, 1e-4f));
                CHECK_THAT (eDist, WithinAbs (0.0f, 1e-4f));
            }
        }
        CAPTURE (shape, maxErr);
        CHECK (maxErr < 1e-4f);
    }
}

TEST_CASE ("Trajectory reverse: Line reverse is a half-period shift (OSD#100)", "[trajectory][reverse]")
{
    // Line is cos (2 pi phase), an even function, so the generic phase flip would change
    // nothing. computeTrajectory shifts it half a period instead (D-19: deliberate, kept).
    const Base bases[] = { { 30.0f, 10.0f, 0.5f }, { -120.0f, -20.0f, 0.8f }, { 0.0f, 0.0f, 0.2f } };

    for (const auto& base : bases)
    {
        for (int k = 0; k <= 100; ++k)
        {
            const float p = (float) k / 100.0f;
            const auto rev = TrajectoryEngine::computeTrajectory (TrajShape::Line, p, base.az, base.el, base.dist, true);
            const auto fwd = TrajectoryEngine::computeTrajectory (TrajShape::Line, std::fmod (p + 0.5f, 1.0f),
                                                                    base.az, base.el, base.dist);
            CAPTURE (base.az, base.el, base.dist, p);
            CHECK_THAT (angularDiff (rev.azDeg, fwd.azDeg), WithinAbs (0.0f, 1e-4f));
            CHECK_THAT (rev.elDeg, WithinAbs (fwd.elDeg, 1e-4f));
            CHECK_THAT (rev.dist, WithinAbs (fwd.dist, 1e-4f));
        }
    }
}

TEST_CASE ("Trajectory reverse: Bounce reverse mirrors the azimuth offset (OSD#100)", "[trajectory][reverse]")
{
    // Bounce deliberately negates the azimuth offset in reverse so the az/el relationship
    // mirrors diagonally (OpenSpatialDelay#100). Intentional exception to D-13 (D-19): kept.
    const Base bases[] = { { 30.0f, 10.0f, 0.5f }, { -40.0f, 0.0f, 0.6f } };

    for (const auto& base : bases)
    {
        for (int k = 0; k <= 100; ++k)
        {
            const float p = (float) k / 100.0f;
            const auto fwd = TrajectoryEngine::computeTrajectory (TrajShape::Bounce, p, base.az, base.el, base.dist);
            const auto rev = TrajectoryEngine::computeTrajectory (TrajShape::Bounce, p, base.az, base.el, base.dist, true);
            CAPTURE (base.az, base.el, base.dist, p);
            CHECK_THAT (rev.azDeg - base.az, WithinAbs (-(fwd.azDeg - base.az), 1e-4f));
            CHECK_THAT (rev.elDeg, WithinAbs (fwd.elDeg, 1e-4f));
            CHECK_THAT (rev.dist, WithinAbs (fwd.dist, 1e-4f));
        }
    }
}

TEST_CASE ("Trajectory tick: all 13 shapes animate forward and reverse through tick()",
           "[trajectory][tick][reverse]")
{
    for (int shape = TrajShape::Bounce; shape <= TrajShape::Triangle; ++shape)
    {
        for (bool reverse : { false, true })
        {
            TrajectoryEngine engine;
            TrajectoryEngine::ObjectInput input;
            input.shape = shape;
            input.speed = 1.0f;
            input.reverse = reverse;
            input.originAz = 20.0f;
            input.originEl = 5.0f;
            input.originDist = 0.5f;

            float firstAz = 0.0f, firstEl = 0.0f, firstDist = 0.0f;
            bool moved = false;

            for (int t = 0; t < 60; ++t)
            {
                engine.tick (0, input, 1.0f / 60.0f);
                const float az = engine.getFinalAz (0);
                const float el = engine.getFinalEl (0);
                const float dist = engine.getFinalDist (0);

                CAPTURE (shape, reverse, t, az, el, dist);
                REQUIRE (engine.isActive (0));
                REQUIRE (inRange (az, el, dist));

                if (t == 0)
                    firstAz = az, firstEl = el, firstDist = dist;
                else if (az != firstAz || el != firstEl || dist != firstDist)
                    moved = true;
            }

            CAPTURE (shape, reverse);
            CHECK (moved);

            if (shape != TrajShape::Random)
            {
                const auto expected = TrajectoryEngine::computeTrajectory (shape, engine.phase_[0],
                                                                            20.0f, 5.0f, 0.5f, reverse);
                CHECK_THAT (angularDiff (engine.getFinalAz (0), expected.azDeg), WithinAbs (0.0f, 1e-4f));
                CHECK_THAT (engine.getFinalEl (0), WithinAbs (expected.elDeg, 1e-4f));
                CHECK_THAT (engine.getFinalDist (0), WithinAbs (expected.dist, 1e-4f));
            }
        }
    }
}

TEST_CASE ("Trajectory tick: Random is seeded, moves and stays in range, forward and reverse",
           "[trajectory][tick][reverse][random]")
{
    for (bool reverse : { false, true })
    {
        TrajectoryEngine engine;
        engine.rng_.setSeed (1234);

        TrajectoryEngine::ObjectInput input;
        input.shape = TrajShape::Random;
        input.speed = 1.0f;
        input.reverse = reverse;
        input.originAz = 0.0f;
        input.originEl = 0.0f;
        input.originDist = 0.5f;

        float minAz = 1e9f, maxAz = -1e9f;
        bool allInRange = true;

        for (int t = 0; t < 600; ++t)
        {
            engine.tick (0, input, 1.0f / 60.0f);
            const float az = engine.getFinalAz (0);
            allInRange = allInRange && inRange (az, engine.getFinalEl (0), engine.getFinalDist (0));
            minAz = std::min (minAz, az);
            maxAz = std::max (maxAz, az);
        }

        CAPTURE (reverse, minAz, maxAz, engine.getState (0).randomTime);
        CHECK (allInRange);
        CHECK (maxAz - minAz > 10.0f);
        if (reverse)
            CHECK (engine.getState (0).randomTime < 0.0f);
        else
            CHECK (engine.getState (0).randomTime > 0.0f);
    }
}

// ============================================================================
// RTSF-02 (SpatialCore#18): the final positions are written by the timer thread
// and read from other threads. [tsan] marks the thread-interaction subset the
// build-tsan gate runs (D-01).
// ============================================================================

TEST_CASE ("TrajectoryEngine: a reader thread sees only finished, in-range positions while the timer thread ticks (RTSF-02)",
           "[trajectory][threads][tsan]")
{
    TrajectoryEngine engine;
    engine.rng_.setSeed (1234);

    constexpr int kRounds = 20000;

    std::atomic<bool> readerStarted { false };
    std::atomic<bool> tickerDone { false };
    int violations = 0;
    long passesDuringTicking = 0;

    // Reader thread: every object, every getter, until the ticker is done.
    std::thread reader ([&]
    {
        readerStarted.store (true);
        while (! tickerDone.load())
        {
            for (int i = 0; i < TrajectoryEngine::kMaxObjects; ++i)
            {
                (void) engine.isActive (i);
                const float az   = engine.getFinalAz (i);
                const float el   = engine.getFinalEl (i);
                const float dist = engine.getFinalDist (i);

                if (! (az >= -180.0f && az <= 180.0f)
                    || ! (el >= -90.0f && el <= 90.0f)
                    || ! (dist >= 0.0f && dist <= 1.0f))
                    ++violations;
            }
            ++passesDuringTicking;
        }
    });

    // Timer thread (the only writer): all 12 objects through all 13 shapes at speed 4.
    std::thread ticker ([&]
    {
        while (! readerStarted.load())
            std::this_thread::yield();

        for (int round = 0; round < kRounds; ++round)
        {
            for (int t = 0; t < TrajectoryEngine::kMaxObjects; ++t)
            {
                TrajectoryEngine::ObjectInput input;
                input.shape = 1 + (t + round / 500) % 13;
                input.speed = 4.0f;
                input.reverse = (t % 2) == 1;
                // Random's raw sum passes 180 from origin 170, so only the wrap brings it back.
                input.originAz = (input.shape == TrajShape::Random) ? 170.0f
                                                                    : 20.0f * (float) t - 110.0f;
                input.originEl = 5.0f;
                input.originDist = 0.5f;
                engine.tick (t, input, 1.0f / 60.0f);
            }

            if (round % 1000 == 999)
                engine.reset (round % TrajectoryEngine::kMaxObjects, 30.0f, 10.0f, 0.4f);
        }

        tickerDone.store (true);
    });

    ticker.join();
    reader.join();

    CAPTURE (violations, passesDuringTicking);
    CHECK (violations == 0);
    CHECK (passesDuringTicking >= 1000);
}

// ============================================================================
// RTSF-02 mixed-tick bound. A reader on another thread may combine values from
// two consecutive 60 Hz ticks (azimuth from one, elevation from the next). Such a
// reading is off by at most one tick of motion, so the largest per-tick step is
// measured here and pinned: "not audible" is a number, not a belief.
//
// Measured (shapes 1..13, speeds 1 and 4, forward and reverse, origin 20/5/0.5,
// 600 ticks at 1/60 s, Random seeded 1234), largest change between two ticks:
//
//   shape            az (deg)   el (deg)   dist
//    1 Bounce          24.000      8.000   0.000
//    2 Circle         178.565      0.000   0.204   <- azimuth flips by about 180 deg once per cycle
//    3 Cross           48.000     32.000   0.000
//    4 Figure8        174.308      0.000   0.203   <- azimuth flips by about 180 deg once per cycle
//    5 Heart           26.170      0.000   0.277
//    6 Helix           24.000    179.877   0.000   <- elevation sawtooth: +90 to -90 at the phase wrap
//    7 Infinity        22.132      0.000   0.188
//    8 Line            23.139      0.000   0.084
//    9 Orbit           24.000      0.000   0.000
//   10 Random          14.633     16.387   0.100
//   11 Spiral          86.538      0.000   0.506   <- large azimuth and distance steps of its own
//   12 Square          50.890      0.000   0.122
//   13 Triangle        56.314      0.000   0.173
//
// What this means: a reader that combines two consecutive ticks is off by at most
// these steps. For the nine smooth shapes (1, 3, 5, 7, 8, 9, 10, 12, 13) that is at
// most 56.4 deg, 32 deg and 0.28; Circle, Figure8, Helix and Spiral have a jump of
// their own, and a mixed reading of them is never worse than the jump the shape
// already makes in its own motion. The constants below are the overall maxima rounded up (next whole degree,
// next 0.01). If a mixed reading is ever judged audible, the escape is a per-object
// seqlock (lock-free, not wait-free), not a mutex.
// ============================================================================

TEST_CASE ("TrajectoryEngine: the largest change between two 60 Hz ticks is pinned for every shape (RTSF-02 mixed-tick bound)",
           "[trajectory][step]")
{
    constexpr float kMaxAzStepDeg = 179.0f;
    constexpr float kMaxElStepDeg = 180.0f;
    constexpr float kMaxDistStep  = 0.51f;

    for (int shape = TrajShape::Bounce; shape <= TrajShape::Triangle; ++shape)
    {
        float shapeAz = 0.0f, shapeEl = 0.0f, shapeDist = 0.0f;

        for (float speed : { 1.0f, 4.0f })
        {
            for (bool reverse : { false, true })
            {
                TrajectoryEngine engine;
                engine.rng_.setSeed (1234);

                TrajectoryEngine::ObjectInput input;
                input.shape = shape;
                input.speed = speed;
                input.reverse = reverse;
                input.originAz = 20.0f;
                input.originEl = 5.0f;
                input.originDist = 0.5f;

                engine.tick (0, input, 1.0f / 60.0f);
                float prevAz = engine.getFinalAz (0);
                float prevEl = engine.getFinalEl (0);
                float prevDist = engine.getFinalDist (0);

                for (int t = 1; t < 600; ++t)
                {
                    engine.tick (0, input, 1.0f / 60.0f);
                    const float az = engine.getFinalAz (0);
                    const float el = engine.getFinalEl (0);
                    const float dist = engine.getFinalDist (0);

                    shapeAz   = std::max (shapeAz,   std::abs (unwrapAzimuthDelta (az - prevAz)));
                    shapeEl   = std::max (shapeEl,   std::abs (el - prevEl));
                    shapeDist = std::max (shapeDist, std::abs (dist - prevDist));
                    prevAz = az; prevEl = el; prevDist = dist;
                }
            }
        }

        CAPTURE (shape, shapeAz, shapeEl, shapeDist);
        CHECK (shapeAz   <= kMaxAzStepDeg);
        CHECK (shapeEl   <= kMaxElStepDeg);
        CHECK (shapeDist <= kMaxDistStep);
    }
}
