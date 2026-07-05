#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Trajectory/DopplerVelocity.h>

using namespace spatialcore;

TEST_CASE ("DopplerVelocity: static position yields zero semitones", "[doppler]")
{
    DopplerVelocity dv;
    dv.reset (0, 0.0f, 0.0f, 0.5f);
    dv.update (0, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f / 60.0f);
    dv.smooth (0);
    REQUIRE (dv.getSmoothedSemitones (0) == 0.0f);
}
