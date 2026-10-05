// SpatialMapComponent behaviour (Phase 4, Plan 04-05, EXTR-05, ROADMAP criterion 4, D-01).
//
// The component is built without a window, driven by synthesized mouse events and painted into a
// juce::Image. Drag cases assert what a host Listener hears; pixel cases assert what the map draws
// (position, distance rings, elevation opacity). Nothing here changes the component: a drag edits
// azimuth and distance only (D-04), and elevation opacity is the existing fill alpha.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "UITestSupport.h"

#include <cmath>

using spatialcore::SpatialMapComponent;
using namespace spatialcore::test;

namespace
{

constexpr int kMapSize = 400;
constexpr float kRadiusPx = (float) kMapSize * 0.45f; // 180 px

// Records what a host sees: counts and the last values.
struct RecordingListener : SpatialMapComponent::Listener
{
    void objectPositionChanged (int index, float az, float dist) override
    {
        ++positionCount;
        lastIndex = index;
        lastAzimuth = az;
        lastDistance = dist;
    }

    void objectSelected (int index) override
    {
        ++selectedCount;
        lastSelected = index;
    }

    int positionCount = 0;
    int selectedCount = 0;
    int lastIndex = -1;
    int lastSelected = -1;
    float lastAzimuth = 0.0f;
    float lastDistance = 0.0f;
};

// A 400x400 map with a recording listener already attached.
struct MapFixture
{
    MapFixture() : map (4)
    {
        map.setSize (kMapSize, kMapSize);
        map.addListener (&listener);
        map.onDragStarted = [this] (int i) { ++dragStarted; lastDragStarted = i; };
        map.onDragEnded   = [this] (int i) { ++dragEnded;   lastDragEnded = i; };
    }

    ~MapFixture() { map.removeListener (&listener); }

    SpatialMapComponent map;
    RecordingListener listener;
    int dragStarted = 0, dragEnded = 0, lastDragStarted = -1, lastDragEnded = -1;
};

// The fill colour of object `i`, as a pixel a few pixels right of the dot centre (inside the
// dot at every elevation used here, clear of the dot's own label which sits above or below it).
juce::Colour dotPixel (const juce::Image& image, juce::Point<float> centre)
{
    return image.getPixelAt (juce::roundToInt (centre.x) + 4, juce::roundToInt (centre.y));
}

} // namespace

TEST_CASE ("Map drag: reports az 90 d 0.8, keeps elevation, fires callbacks", "[ui][map][drag]")
{
    MapFixture f;
    f.map.setObjectState (0, 0.0f, 30.0f, 0.5f, true);

    dragObject (f.map, 0, mapPointFor (f.map, 90.0f, 0.8f));

    CAPTURE (f.listener.lastAzimuth, f.listener.lastDistance);
    CHECK (f.listener.lastIndex == 0);
    CHECK (f.listener.lastAzimuth == Catch::Approx (90.0f).margin (0.01));
    CHECK (f.listener.lastDistance == Catch::Approx (0.8f).margin (0.001));

    CHECK (f.listener.selectedCount == 1);
    CHECK (f.listener.lastSelected == 0);
    CHECK (f.dragStarted == 1);
    CHECK (f.lastDragStarted == 0);
    CHECK (f.dragEnded == 1);
    CHECK (f.lastDragEnded == 0);

    // The drag stays azimuth + distance (D-04).
    CHECK (f.map.getObjectElevation (0) == 30.0f);

    const auto pos = f.map.getObjectScreenPos (0);
    const auto want = mapPointFor (f.map, 90.0f, 0.8f);
    CAPTURE (pos.x, pos.y, want.x, want.y);
    CHECK (pos.getDistanceFrom (want) <= 0.5f);
}

TEST_CASE ("Map drag: outside the outer ring clamps distance to 1", "[ui][map][drag]")
{
    MapFixture f;
    f.map.setObjectState (0, 0.0f, 30.0f, 0.5f, true);

    dragObject (f.map, 0, { 5.0f, 200.0f });

    CAPTURE (f.listener.lastAzimuth, f.listener.lastDistance);
    CHECK (f.listener.lastDistance == 1.0f);
    CHECK (f.listener.lastAzimuth == Catch::Approx (90.0f).margin (0.5));

    // The exact centre: distance 0. Azimuth is atan2 (-0, -0), -180 by IEEE rules, so it is not
    // asserted (RESEARCH A4).
    dragObject (f.map, 0, { (float) kMapSize * 0.5f, (float) kMapSize * 0.5f });

    CAPTURE (f.listener.lastDistance);
    CHECK (f.listener.lastDistance == 0.0f);
}

TEST_CASE ("Map drag: empty space and disabled objects do not move anything", "[ui][map][drag]")
{
    MapFixture f;
    f.map.setObjectState (0, 0.0f, 30.0f, 0.5f, true);
    f.map.setObjectState (1, 90.0f, 0.0f, 0.5f, false); // disabled, at a known place

    // Far from every object.
    const juce::Point<float> empty { 350.0f, 350.0f };
    f.map.mouseDown (makeMouseEvent (f.map, empty, empty, false));
    f.map.mouseDrag (makeMouseEvent (f.map, { 300.0f, 300.0f }, empty, true));
    f.map.mouseUp (makeMouseEvent (f.map, { 300.0f, 300.0f }, empty, true));

    CHECK (f.listener.selectedCount == 0);
    CHECK (f.listener.positionCount == 0);
    CHECK (f.dragStarted == 0);
    CHECK (f.dragEnded == 0);

    // The disabled object cannot be picked up, even on its own pixel.
    dragObject (f.map, 1, mapPointFor (f.map, 0.0f, 0.9f));

    CHECK (f.listener.selectedCount == 0);
    CHECK (f.listener.positionCount == 0);
    CHECK_FALSE (f.map.isObjectEnabled (1));
    CHECK (f.map.getObjectScreenPos (1).getDistanceFrom (mapPointFor (f.map, 90.0f, 0.5f)) <= 0.5f);
}

TEST_CASE ("Map pixels: the dot moves on screen with the drag", "[ui][map][pixels]")
{
    MapFixture f;
    f.map.setObjectState (0, 0.0f, 30.0f, 0.5f, true);
    const auto colour = SpatialMapComponent::objectColours[0];

    const auto oldPos = f.map.getObjectScreenPos (0);
    const auto before = dotPixel (renderToImage (f.map), oldPos);
    CAPTURE (before.toDisplayString (true), colour.toDisplayString (true));
    CHECK (maxChannelDiff (before, colour) <= 8);

    dragObject (f.map, 0, mapPointFor (f.map, 90.0f, 0.8f));

    const auto image = renderToImage (f.map);
    const auto newPos = f.map.getObjectScreenPos (0);
    const auto atNew = dotPixel (image, newPos);
    const auto atOld = dotPixel (image, oldPos);
    CAPTURE (atNew.toDisplayString (true), atOld.toDisplayString (true));
    CHECK (maxChannelDiff (atNew, colour) <= 8);
    CHECK (maxChannelDiff (atOld, colour) > 8);
}

TEST_CASE ("Map pixels: elevation opacity", "[ui][map][pixels]")
{
    MapFixture f;
    f.map.setObjectState (0, 90.0f, 80.0f, 0.5f, true);
    f.map.setObjectState (1, -90.0f, -80.0f, 0.5f, true);

    const auto image = renderToImage (f.map);
    const auto above = dotPixel (image, f.map.getObjectScreenPos (0));
    const auto below = dotPixel (image, f.map.getObjectScreenPos (1));
    CAPTURE (above.toDisplayString (true), below.toDisplayString (true),
             rgbSum (above), rgbSum (below));

    CHECK (maxChannelDiff (above, SpatialMapComponent::objectColours[0]) <= 8);
    CHECK ((float) rgbSum (below) < 0.6f * (float) rgbSum (above));
}

TEST_CASE ("Map pixels: distance rings and dot radius", "[ui][map][pixels]")
{
    MapFixture f;
    const float cx = (float) kMapSize * 0.5f;
    const float cy = (float) kMapSize * 0.5f;

    // An object at azimuth 0 sits d * radius above the centre.
    for (float d : { 0.25f, 0.5f, 0.75f, 1.0f })
    {
        f.map.setObjectState (0, 0.0f, 30.0f, d, true);
        const auto pos = f.map.getObjectScreenPos (0);
        CAPTURE (d, pos.x, pos.y);
        CHECK (pos.x == Catch::Approx (cx).margin (0.5));
        CHECK ((cy - pos.y) == Catch::Approx (d * kRadiusPx).margin (0.5));

        // And the painted dot is there.
        const auto px = dotPixel (renderToImage (f.map), pos);
        CAPTURE (px.toDisplayString (true));
        CHECK (maxChannelDiff (px, SpatialMapComponent::objectColours[0]) <= 8);
    }

    // With no object on the map, each ring is brighter than the void halfway to the next ring in.
    // The ray is 70 degrees below the +x axis on screen: clear of the ring labels (30 and 210
    // degrees), the cardinal marks and the dashed crosshairs (multiples of 45 degrees).
    f.map.setObjectState (0, 0.0f, 30.0f, 0.5f, false);
    const auto image = renderToImage (f.map);
    const float angle = juce::degreesToRadians (70.0f);

    auto pixelOnRay = [&] (float r)
    {
        return image.getPixelAt (juce::roundToInt (cx + std::cos (angle) * r),
                                 juce::roundToInt (cy + std::sin (angle) * r));
    };

    for (float d : { 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const float ringR = d * kRadiusPx;
        int brightest = 0;
        for (float r = ringR - 1.0f; r <= ringR + 1.0f; r += 0.25f)
            brightest = std::max (brightest, rgbSum (pixelOnRay (r)));

        const int voidSum = rgbSum (pixelOnRay (ringR - 0.5f * 0.25f * kRadiusPx));
        CAPTURE (d, ringR, brightest, voidSum);
        CHECK (brightest >= voidSum + 15);
    }
}
