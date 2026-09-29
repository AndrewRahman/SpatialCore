#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
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
