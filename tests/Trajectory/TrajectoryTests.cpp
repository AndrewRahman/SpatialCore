#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

TEST_CASE("Orbit at phase 0 starts at base azimuth", "[trajectory][characterization]")
{
    auto r = TrajectoryEngine::computeTrajectory (9 /* Orbit */, 0.0f, 90.0f, 10.0f, 0.5f);
    CHECK_THAT (r.azDeg, WithinAbs (90.0f, 0.01f));
    CHECK (r.controlsAz);
}
