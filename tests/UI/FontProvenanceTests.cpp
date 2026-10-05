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

// Documented exemption (D-22, rejected at the Phase 4 review): the user kept the original SAVE PRESET
// title, a typeface-less bold font that resolves through the default look-and-feel. It is the one UI
// font that does not come from SpatialCoreUIFontData, so it makes exactly the one request it always
// made. A second request, or none, means the title changed without a new ruling.
TEST_CASE ("Fonts: the save-preset title keeps its original single font request", "[ui][fonts][spy]")
{
    ScopedSpyDefault spyDefault;

    PresetSaveOverlay overlay;
    overlay.setSize (260, 130);
    spyDefault.reset(); // the constructor may build child widgets; the title paint is what is measured

    (void) paintOverlay (overlay);
    INFO ("requested by the overlay title: " << describe (spyDefault.spy.requested));
    CHECK (spyDefault.spy.calls == 1);
}

TEST_CASE ("Fonts: every embedded font resource equals its source file", "[ui][fonts][bytes]")
{
    const std::string fontDir = std::string (SPATIALCORE_SOURCE_DIR) + "/fonts/";

    // IN-07: the expected count is the number of .ttf files in fonts/ (the CMake font list embeds each
    // of them), not a literal, so adding a font does not break this with an unexplained "8".
    const int ttfFiles = juce::File (fontDir).findChildFiles (juce::File::findFiles, false, "*.ttf").size();
    INFO ("the embedded font list has " << SpatialCoreUIFontData::namedResourceListSize
          << " entries but fonts/ holds " << ttfFiles << " .ttf files: "
          << "add the new font to the SpatialCoreUIFontData source list in CMakeLists.txt (or remove it from fonts/)");
    REQUIRE (ttfFiles > 0);
    REQUIRE (SpatialCoreUIFontData::namedResourceListSize == ttfFiles);

    for (int i = 0; i < SpatialCoreUIFontData::namedResourceListSize; ++i)
    {
        const char* name = SpatialCoreUIFontData::namedResourceList[i];
        const char* original = SpatialCoreUIFontData::getNamedResourceOriginalFilename (name);
        REQUIRE (original != nullptr);
        INFO ("resource " << name << " from " << original);

        int size = 0;
        const char* data = SpatialCoreUIFontData::getNamedResource (name, size);
        REQUIRE (data != nullptr);

        std::ifstream in (fontDir + original, std::ios::binary);
        REQUIRE (in.good());
        std::vector<char> file ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());

        REQUIRE ((int) file.size() == size);
        CHECK (std::memcmp (file.data(), data, file.size()) == 0);
    }
}

TEST_CASE ("Fonts: SMLLookAndFeel loads all seven embedded typefaces", "[ui][fonts][sml]")
{
    SMLLookAndFeel sml;
    CHECK (sml.dmSansRegular != nullptr);
    CHECK (sml.dmSansMedium != nullptr);
    CHECK (sml.dmSansBold != nullptr);
    CHECK (sml.jetbrainsRegular != nullptr);
    CHECK (sml.jetbrainsMedium != nullptr);
    CHECK (sml.jetbrainsBold != nullptr);
    CHECK (sml.robotoMedium != nullptr);
}

namespace
{

// D-05 code rule: a request for an SML font by family name would find an installed copy of that
// family on a machine that has one and nothing on a machine that does not.
const std::regex& familyNameRule()
{
    static const std::regex rule (
        "\"[^\"]*(DM Sans|JetBrains|Roboto)[^\"]*\""
        "|withName|setTypefaceName|getDefaultSansSerifFontName"
        "|Font *\\( *\"|FontOptions *\\( *\""
        "|findAllTypefaceNames|getTypefaceName");
    return rule;
}

std::vector<juce::File> uiSourceFiles()
{
    std::vector<juce::File> files;
    const juce::File root (SPATIALCORE_SOURCE_DIR);
    for (auto* sub : { "src/UI", "include/SpatialCore/UI" })
        for (auto& f : root.getChildFile (sub).findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
            files.push_back (f);
    return files;
}

// IN-07: the family-name rule is about code, not prose. Blanks out // and /* */ comments (and keeps
// string and character literals, which is where a family name would be) so a comment that merely
// mentions "DM Sans" or getTypefaceName cannot fail it. Newlines are kept, so line numbers still match.
std::string stripComments (const std::string& src)
{
    std::string out;
    out.reserve (src.size());
    enum class State { code, lineComment, blockComment, string, character } state = State::code;

    for (size_t i = 0; i < src.size(); ++i)
    {
        const char c = src[i];
        const char next = i + 1 < src.size() ? src[i + 1] : '\0';

        switch (state)
        {
            case State::code:
                if (c == '/' && next == '/')       { state = State::lineComment; ++i; }
                else if (c == '/' && next == '*')  { state = State::blockComment; ++i; }
                else
                {
                    if (c == '"')  state = State::string;
                    if (c == '\'') state = State::character;
                    out += c;
                }
                break;
            case State::lineComment:
                if (c == '\n') { state = State::code; out += c; }
                break;
            case State::blockComment:
                if (c == '*' && next == '/') { state = State::code; ++i; }
                else if (c == '\n')          { out += c; }
                break;
            case State::string:
            case State::character:
                out += c;
                if (c == '\\' && next != '\0') { out += next; ++i; }
                else if ((state == State::string && c == '"') || (state == State::character && c == '\''))
                    state = State::code;
                break;
        }
    }
    return out;
}

} // namespace

TEST_CASE ("Fonts: the comment stripper keeps code and drops prose", "[ui][fonts][rule]")
{
    CHECK (stripComments ("a(); // getTypefaceName\nb();") == "a(); \nb();");
    CHECK (stripComments ("a(); /* \"DM Sans\" */ b();") == "a();  b();");
    CHECK (stripComments ("x = \"http://DM Sans\";") == "x = \"http://DM Sans\";");   // // inside a string stays
    CHECK (stripComments ("c = '\"'; // tail") == "c = '\"'; ");
    CHECK (stripComments ("s = \"a\\\"//b\";") == "s = \"a\\\"//b\";");           // an escaped quote does not end the string
}

TEST_CASE ("Fonts: no UI code asks for an SML font by family name", "[ui][fonts][rule]")
{
    // Positive controls: the rule is live, so an empty result means something.
    CHECK (std::regex_search (std::string ("juce::FontOptions (\"DM Sans\")"), familyNameRule()));
    CHECK (std::regex_search (std::string ("auto n = f.getTypefaceName();"), familyNameRule()));
    CHECK (std::regex_search (std::string ("juce::Font (\"Arial\", 12.0f, 0)"), familyNameRule()));
    CHECK_FALSE (std::regex_search (std::string ("juce::FontOptions (monoBold_).withHeight (10.0f)"), familyNameRule()));

    const auto files = uiSourceFiles();
    REQUIRE (files.size() >= 10);

    int typefaceCreations = 0;
    for (auto& f : files)
    {
        std::istringstream lines (stripComments (f.loadFileAsString().toStdString()));
        std::string line;
        int lineNo = 0;
        std::string offenders;   // one assertion per file, listing every offending line
        while (std::getline (lines, line))
        {
            ++lineNo;
            if (std::regex_search (line, familyNameRule()))
                offenders += "\n  line " + std::to_string (lineNo) + ": " + line;

            if (line.find ("createSystemTypefaceFor") != std::string::npos)
            {
                ++typefaceCreations;
                if (line.find ("SpatialCoreUIFontData::") == std::string::npos)
                    offenders += "\n  line " + std::to_string (lineNo) + " (typeface not from SpatialCoreUIFontData): " + line;
            }
        }

        INFO (f.getFullPathName().toStdString() << offenders);
        CHECK (offenders.empty());
    }

    // SMLLookAndFeel loads 7 and the map 3 (the shared MonoFaces): the scan really saw the call sites.
    CHECK (typefaceCreations >= 7);
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
