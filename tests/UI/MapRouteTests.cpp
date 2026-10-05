// Map route (Phase 4, Plan 04-05, ROADMAP criterion 4, D-01 / D-11 / D-12 / D-18).
//
// A host embeds SpatialMapComponent, the user drags an object, and the sound moves. The glue
// below is what a plugin writes: a Listener that copies objectPositionChanged into the render
// sources. Proof is the rendered L/R level through RenderEngine, never that a callback fired.

#include <catch2/catch_test_macros.hpp>

#include "UITestSupport.h"
#include "../Support/RouteRenderRig.h"

#include <cmath>

using spatialcore::SpatialMapComponent;
using spatialcore::test::RouteRenderRig;

namespace
{

// Host-side glue. Elevation is passed as NaN so the rig keeps it: the map drag edits azimuth and
// distance only (D-04).
struct MapToRigGlue : SpatialMapComponent::Listener
{
    explicit MapToRigGlue (RouteRenderRig& r) : rig (r) {}

    void objectPositionChanged (int index, float azimuthDeg, float distance) override
    {
        rig.mergeObject (index, azimuthDeg, std::nanf (""), distance);
    }

    void objectSelected (int) override { ++selectedCount; }

    RouteRenderRig& rig;
    int selectedCount = 0;
};

} // namespace

TEST_CASE ("Map route: dragging an object to the left makes the left channel louder",
           "[ui][route][map][tracer]")
{
    SpatialMapComponent map (4);
    map.setSize (400, 400);
    map.setObjectState (0, 0.0f, 30.0f, 0.5f, true);

    RouteRenderRig rig (spatialcore::OutputFormat::Binaural, 2);
    rig.setObject (0, 0.0f, 30.0f, 0.5f);

    MapToRigGlue glue (rig);
    map.addListener (&glue);

    spatialcore::test::dragObject (map, 0, spatialcore::test::mapPointFor (map, 90.0f, 0.8f));
    const float leftRatio = rig.leftRightRatio();
    CAPTURE (leftRatio);
    REQUIRE (leftRatio >= 2.0f);

    spatialcore::test::dragObject (map, 0, spatialcore::test::mapPointFor (map, -90.0f, 0.8f));
    const float rightRatio = rig.leftRightRatio();
    CAPTURE (rightRatio);
    REQUIRE (rightRatio <= 0.5f);

    CHECK (glue.selectedCount == 2);
    CHECK (rig.object (0).elevationDeg == 30.0f);

    map.removeListener (&glue);
}
