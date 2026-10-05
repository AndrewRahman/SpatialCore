// Font provenance (Phase 4, Plan 04-06, DATA-02, D-05, D-06, D-22).
//
// "SML fonts render on a machine with no SML font installed" cannot be shown by a screenshot on
// a Mac that has DM Sans and JetBrains Mono installed, so these tests prove it by provenance:
// a spy look-and-feel installed as the JUCE default sees every typeface request a paint makes
// (JUCE 9 resolves a typeface-less Font through the default look-and-feel, never through a
// component-level one), the embedded bytes equal the source fonts, and a code rule shows that
// no UI code asks for an SML font by family name.

#include <catch2/catch_test_macros.hpp>

#include <SpatialCore/UI/PresetBrowser.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>

#include "SpatialCoreUIFontData.h"
#include "UITestSupport.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using spatialcore::PresetSaveOverlay;
using spatialcore::SMLLookAndFeel;
using spatialcore::SpatialMapComponent;
using namespace spatialcore::test;

namespace
{

// Counts every typeface the default look-and-feel is asked for, then defers to the base class.
struct SpyLookAndFeel : juce::LookAndFeel_V4
{
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        ++calls;
        requested.add (font.getTypefaceName());
        return juce::LookAndFeel_V4::getTypefaceForFont (font);
    }

    int calls = 0;
    juce::StringArray requested;
};

// Installs a spy as the JUCE default look-and-feel and clears the typeface cache (the cache hides
// repeated requests, RESEARCH Pitfall 4). Restores the default on destruction, before the spy dies.
struct ScopedSpyDefault
{
    ScopedSpyDefault()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&spy);
        juce::Typeface::clearTypefaceCache();
    }

    ~ScopedSpyDefault()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void reset()
    {
        spy.calls = 0;
        spy.requested.clear();
        juce::Typeface::clearTypefaceCache();
    }

    SpyLookAndFeel spy;
};

// A 400x400 map whose state reaches all five font sites: ring labels and cardinals always, dot
// numbers for enabled objects, the elevation label for the selected object with |elevation| > 1,
// and the OSC label for an object with its override set.
std::unique_ptr<SpatialMapComponent> makeFontTestMap()
{
    auto map = std::make_unique<SpatialMapComponent> (4);
    map->setSize (400, 400);
    map->setObjectState (0, 30.0f, 30.0f, 0.5f, true);
    map->setObjectState (1, -120.0f, -40.0f, 0.7f, true);
    map->setSelectedObject (0);
    map->setOscOverride (0, true);
    return map;
}

std::string describe (const juce::StringArray& names)
{
    return names.joinIntoString (", ").toStdString();
}

bool writePng (const juce::Image& image, const juce::File& file)
{
    juce::PNGImageFormat png;
    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && png.writeImageToStream (image, out);
}

// Calls the overlay's own paint (not its child editor and buttons) into an image.
juce::Image paintOverlay (PresetSaveOverlay& overlay)
{
    juce::Image image (juce::Image::ARGB, overlay.getWidth(), overlay.getHeight(), true);
    juce::Graphics g (image);
    overlay.paint (g);
    return image;
}

} // namespace

TEST_CASE ("Fonts: the map asks the default look-and-feel for no font, with or without SML",
           "[ui][fonts][spy][tracer]")
{
    ScopedSpyDefault spyDefault;
    auto map = makeFontTestMap();

    // No look-and-feel on the component: the map must still need nothing from the system.
    (void) renderToImage (*map);
    INFO ("requested with no SML look-and-feel: " << describe (spyDefault.spy.requested));
    CHECK (spyDefault.spy.calls == 0);

    // SML set on the component, the way OpenSpatialDelay installs it.
    spyDefault.reset();
    SMLLookAndFeel sml;
    map->setLookAndFeel (&sml);
    (void) renderToImage (*map);
    INFO ("requested with SML on the component: " << describe (spyDefault.spy.requested));
    CHECK (spyDefault.spy.calls == 0);

    map->setLookAndFeel (nullptr);
}

TEST_CASE ("Fonts: map render is byte-identical with and without the SML look-and-feel",
           "[ui][fonts][identical]")
{
    // D-06: the map owns its fonts, so the look-and-feel set on it changes nothing. Pixel for pixel.
    SMLLookAndFeel sml;
    auto withSml = makeFontTestMap();
    withSml->setLookAndFeel (&sml);
    const auto imageSml = renderToImage (*withSml);

    auto without = makeFontTestMap();
    const auto imagePlain = renderToImage (*without);

    REQUIRE (imageSml.getWidth() == imagePlain.getWidth());
    REQUIRE (imageSml.getHeight() == imagePlain.getHeight());

    int differing = 0;
    for (int y = 0; y < imageSml.getHeight(); ++y)
        for (int x = 0; x < imageSml.getWidth(); ++x)
            if (imageSml.getPixelAt (x, y) != imagePlain.getPixelAt (x, y))
                ++differing;

    CHECK (differing == 0);

    withSml->setLookAndFeel (nullptr);
}

TEST_CASE ("Fonts: capture map and overlay renders for the D-06 and D-22 identity checks",
           "[.][ui-capture]")
{
    const char* dir = std::getenv ("SC_UI_CAPTURE_DIR");
    if (dir == nullptr || *dir == '\0')
        SKIP ("SC_UI_CAPTURE_DIR is not set");

    juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile (dir);
    REQUIRE (folder.createDirectory().wasOk());

    // Map: SML on the component only (OSD's setup).
    {
        SMLLookAndFeel sml;
        auto map = makeFontTestMap();
        map->setLookAndFeel (&sml);
        CHECK (writePng (renderToImage (*map), folder.getChildFile ("map-sml-component.png")));
        map->setLookAndFeel (nullptr);
    }

    // Map: SML installed as the default look-and-feel, nothing on the component.
    {
        SMLLookAndFeel sml;
        juce::LookAndFeel::setDefaultLookAndFeel (&sml);
        juce::Typeface::clearTypefaceCache();
        auto map = makeFontTestMap();
        CHECK (writePng (renderToImage (*map), folder.getChildFile ("map-sml-default.png")));
        map.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    // Overlay title: SML as the default look-and-feel.
    {
        SMLLookAndFeel sml;
        juce::LookAndFeel::setDefaultLookAndFeel (&sml);
        juce::Typeface::clearTypefaceCache();
        PresetSaveOverlay overlay;
        overlay.setSize (260, 130);
        CHECK (writePng (paintOverlay (overlay), folder.getChildFile ("overlay-sml-default.png")));
        overlay.setLookAndFeel (nullptr);
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    // Overlay title: SML on the component only (OSD's setup).
    {
        SMLLookAndFeel sml;
        juce::Typeface::clearTypefaceCache();
        PresetSaveOverlay overlay;
        overlay.setLookAndFeel (&sml);
        overlay.setSize (260, 130);
        CHECK (writePng (paintOverlay (overlay), folder.getChildFile ("overlay-sml-component.png")));
        overlay.setLookAndFeel (nullptr);
    }
}
