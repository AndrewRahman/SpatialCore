#pragma once

// Shared helpers for the headless UI tests (Phase 4, Plan 04-05, D-01).
//
// Components are built without a window, driven with synthesized mouse events and painted into a
// juce::Image. Nothing here pumps the message loop. Every helper needs the JUCE GUI initialiser,
// which UITestMain.cpp holds for the whole test run.

#include <SpatialCore/UI/SpatialMapComponent.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace spatialcore::test
{

// A MouseEvent aimed at `c`, built the way a real input source would (JUCE 9 constants).
inline juce::MouseEvent makeMouseEvent (juce::Component& c,
                                        juce::Point<float> pos,
                                        juce::Point<float> downPos,
                                        bool dragged)
{
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                             pos,
                             juce::ModifierKeys(),
                             juce::MouseInputSource::defaultPressure,
                             juce::MouseInputSource::defaultOrientation,
                             juce::MouseInputSource::defaultRotation,
                             juce::MouseInputSource::defaultTiltX,
                             juce::MouseInputSource::defaultTiltY,
                             &c,
                             &c,
                             juce::Time::getCurrentTime(),
                             downPos,
                             juce::Time::getCurrentTime(),
                             1,
                             dragged);
}

// Paints the component and its children into an ARGB image of the component's size.
inline juce::Image renderToImage (juce::Component& c)
{
    juce::Image image (juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
    juce::Graphics g (image);
    c.paintEntireComponent (g, false);
    return image;
}

inline int rgbSum (juce::Colour c)
{
    return (int) c.getRed() + (int) c.getGreen() + (int) c.getBlue();
}

// Largest per-channel difference between two colours.
inline int maxChannelDiff (juce::Colour a, juce::Colour b)
{
    return std::max ({ std::abs ((int) a.getRed()   - (int) b.getRed()),
                       std::abs ((int) a.getGreen() - (int) b.getGreen()),
                       std::abs ((int) a.getBlue()  - (int) b.getBlue()) });
}

// The map's own geometry (SpatialMapComponent.cpp spatialToPixel): radius is 0.45 of the shorter
// side, and positive azimuth is the left of the screen and of the sound.
inline juce::Point<float> mapPointFor (const juce::Component& map, float azDeg, float dist)
{
    const float radius = (float) std::min (map.getWidth(), map.getHeight()) * 0.45f;
    const float cx = (float) map.getWidth() * 0.5f;
    const float cy = (float) map.getHeight() * 0.5f;
    const float az = juce::degreesToRadians (azDeg);
    return { cx - std::sin (az) * dist * radius, cy - std::cos (az) * dist * radius };
}

// Mouse-down on the object, one drag to `to`, mouse-up.
inline void dragObject (SpatialMapComponent& map, int index, juce::Point<float> to)
{
    const auto from = map.getObjectScreenPos (index);
    map.mouseDown (makeMouseEvent (map, from, from, false));
    map.mouseDrag (makeMouseEvent (map, to, from, true));
    map.mouseUp (makeMouseEvent (map, to, from, true));
}

} // namespace spatialcore::test
