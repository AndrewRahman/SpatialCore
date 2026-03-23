#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <string>

using namespace spatialcore;
using Catch::Approx;

TEST_CASE("TrajectoryEngine: 14 shapes (including None)", "[trajectory]")
{
    REQUIRE(TrajectoryEngine::getNumShapes() == 14);
}

TEST_CASE("TrajectoryEngine: shape names are valid", "[trajectory]")
{
    REQUIRE(std::string(TrajectoryEngine::getShapeName(TrajectoryShape::None)) == "None");
    REQUIRE(std::string(TrajectoryEngine::getShapeName(TrajectoryShape::Orbit)) == "Orbit");
    REQUIRE(std::string(TrajectoryEngine::getShapeName(TrajectoryShape::Triangle)) == "Triangle");
}

TEST_CASE("TrajectoryEngine: None shape returns base position", "[trajectory]")
{
    auto result = TrajectoryEngine::compute(TrajectoryShape::None, 0.5f, 45.0f, 10.0f, 0.8f);
    REQUIRE(result.azDeg == Approx(45.0f));
    REQUIRE(result.elDeg == Approx(10.0f));
    REQUIRE(result.dist  == Approx(0.8f));
}

TEST_CASE("TrajectoryEngine: stub returns base position for any shape", "[trajectory]")
{
    auto result = TrajectoryEngine::compute(TrajectoryShape::Circle, 0.25f, 90.0f, 0.0f, 1.0f);
    REQUIRE(result.azDeg == Approx(90.0f));
    REQUIRE(result.elDeg == Approx(0.0f));
    REQUIRE(result.dist  == Approx(1.0f));
}
