// UI umbrella header (Phase 6, Plan 06-01, INTG-01, D-02).
//
// <SpatialCore/SpatialCoreUI.h> is the one include a consumer needs for the shared UI library: the
// spatial map, the SML look-and-feel and the widgets. tests/Core/UmbrellaTests.cpp checks that the
// umbrella names every UI header; this file proves it compiles on its own and that what it gives a
// consumer works: a map built, painted with the SML look-and-feel, and torn down in the right order.
//
// NOTHING may be included before the next line. Being the first include of this translation unit
// is what proves the UI umbrella compiles on its own.
#include <SpatialCore/SpatialCoreUI.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Umbrella: the UI umbrella alone builds and paints the map with the SML look-and-feel",
           "[ui][umbrella]")
{
    // The look-and-feel is declared first so it outlives the map.
    spatialcore::SMLLookAndFeel laf;
    spatialcore::SpatialMapComponent map (2);

    map.setLookAndFeel (&laf);
    map.setSize (240, 240);

    const juce::Image snapshot = map.createComponentSnapshot (map.getLocalBounds());
    CHECK (snapshot.isValid());
    CHECK (snapshot.getWidth() == 240);

    // A component must not keep a pointer to a look-and-feel that is about to be destroyed.
    map.setLookAndFeel (nullptr);
}
