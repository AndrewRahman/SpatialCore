// Multi-map typeface sharing (Phase 5, Plan 05-07, VERIFY-02, D-06, D-12).
//
// OpenSpatialDelay#131, as the issue describes it: with several plugin windows open the host
// crashed in a CoreGraphics glyph call, and the issue's own analysis counted 56 typefaces for 8
// instances, a set of embedded faces built again by every instance. SpatialMapComponent now shares
// one set of the three embedded JetBrains Mono faces across every live map
// (juce::SharedResourcePointer<MonoFaces>, D-06). These tests keep it that way: many maps alive and
// painting at once, a construction counter that proves one set, and a face identity that is the
// same object in every map.
//
// Scope (D-12): this is a test of the map only. SMLLookAndFeel and every rendered pixel are
// unchanged by this plan; the two accessors it adds on the map are read-only and test-only.
//
// UI code is message-thread code. The test thread is the message thread here (the JUCE GUI
// initialiser is held by UITestMain.cpp), and maps are created, painted and destroyed on it only.

#include <catch2/catch_test_macros.hpp>

#include "UITestSupport.h"

#include <memory>
#include <vector>

using spatialcore::SpatialMapComponent;
using namespace spatialcore::test;

namespace
{

// A 400x400 map with 12 objects, four enabled at distinct positions, one of them selected with an
// OSC override, so the ring labels, cardinals, dot numbers, elevation label and OSC label (all five
// font sites) are painted.
std::unique_ptr<SpatialMapComponent> makeMap()
{
    auto map = std::make_unique<SpatialMapComponent> (12);
    map->setSize (400, 400);
    map->setObjectState (0,   30.0f,  30.0f, 0.5f, true);
    map->setObjectState (1, -120.0f, -40.0f, 0.7f, true);
    map->setObjectState (2,   75.0f,   0.0f, 0.3f, true);
    map->setObjectState (3,  -20.0f,  15.0f, 0.9f, true);
    map->setSelectedObject (0);
    map->setOscOverride (0, true);
    return map;
}

int countDifferingPixels (const juce::Image& a, const juce::Image& b)
{
    if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight())
        return -1;

    int differing = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (a.getPixelAt (x, y) != b.getPixelAt (x, y))
                ++differing;

    return differing;
}

} // namespace

TEST_CASE ("Multi-map: sixteen live maps share one face set and paint identically (Spatial-Media-Lab/OpenSpatialDelay#131)",
           "[ui][multi][tracer]")
{
    constexpr int numMaps = 16;

    // No map is alive here: every UI case builds and destroys its own.
    const int countBefore = SpatialMapComponent::getSharedFaceConstructionCountForTesting();

    std::vector<std::unique_ptr<SpatialMapComponent>> maps;
    for (int i = 0; i < numMaps; ++i)
        maps.push_back (makeMap());

    // One set of faces for sixteen maps, not sixteen sets.
    CHECK (SpatialMapComponent::getSharedFaceConstructionCountForTesting() - countBefore == 1);

    const auto* identity = maps.front()->getSharedFaceIdentityForTesting();
    CHECK (identity != nullptr);

    int mapsWithOtherIdentity = 0;
    for (const auto& map : maps)
        if (map->getSharedFaceIdentityForTesting() != identity)
            ++mapsWithOtherIdentity;
    CHECK (mapsWithOtherIdentity == 0);

    // Every map paints, and paints the same image as the first, pixel for pixel.
    const auto reference = renderToImage (*maps.front());
    REQUIRE (reference.getWidth() == 400);
    REQUIRE (reference.getHeight() == 400);

    int mapsWithDifferingImage = 0;
    int differingPixels = 0;
    for (const auto& map : maps)
    {
        const int differing = countDifferingPixels (renderToImage (*map), reference);
        if (differing != 0)
        {
            ++mapsWithDifferingImage;
            differingPixels += differing;
        }
    }
    INFO ("maps whose image differs from the first: " << mapsWithDifferingImage
          << ", differing pixels in total: " << differingPixels);
    CHECK (mapsWithDifferingImage == 0);
    CHECK (differingPixels == 0);

    // Every map destroyed releases the faces; the next map builds them once more and paints the same.
    maps.clear();
    const int countAfterClear = SpatialMapComponent::getSharedFaceConstructionCountForTesting();
    CHECK (countAfterClear - countBefore == 1);

    auto again = makeMap();
    CHECK (SpatialMapComponent::getSharedFaceConstructionCountForTesting() - countAfterClear == 1);
    CHECK (countDifferingPixels (renderToImage (*again), reference) == 0);
}

TEST_CASE ("Multi-map: maps created, painted and destroyed beside four live maps never crash and keep one face set",
           "[ui][multi][churn]")
{
    constexpr int numLongLived = 4;
    constexpr int rounds = 200;

    std::vector<std::unique_ptr<SpatialMapComponent>> longLived;
    std::vector<juce::Image> references;
    for (int i = 0; i < numLongLived; ++i)
    {
        longLived.push_back (makeMap());
        // Give each long-lived map its own look so a mix-up between maps would show.
        longLived.back()->setObjectState (4, 10.0f + 40.0f * (float) i, 5.0f * (float) i, 0.6f, true);
        references.push_back (renderToImage (*longLived.back()));
    }

    const int countBefore = SpatialMapComponent::getSharedFaceConstructionCountForTesting();

    int mismatchedRepaints = 0;
    int badTransientPaints = 0;
    for (int round = 0; round < rounds; ++round)
    {
        auto transient = makeMap();
        const auto transientImage = renderToImage (*transient);
        if (transientImage.getWidth() != 400)
            ++badTransientPaints; // a paint that produced no image of the map size

        for (int i = 0; i < numLongLived; ++i)
            if (countDifferingPixels (renderToImage (*longLived[(size_t) i]), references[(size_t) i]) != 0)
                ++mismatchedRepaints;
        // `transient` is destroyed here, while the long-lived maps keep the faces alive.
    }

    INFO ("long-lived repaints that differed from their reference: " << mismatchedRepaints);
    CHECK (badTransientPaints == 0);
    CHECK (SpatialMapComponent::getSharedFaceConstructionCountForTesting() == countBefore);
    CHECK (mismatchedRepaints == 0);

    for (int i = 0; i < numLongLived; ++i)
        CHECK (countDifferingPixels (renderToImage (*longLived[(size_t) i]), references[(size_t) i]) == 0);
}
