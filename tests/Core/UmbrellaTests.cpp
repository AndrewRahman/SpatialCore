// Umbrella headers (Phase 6, Plan 06-01, INTG-01, D-02, audit W2).
//
// A consumer includes two umbrellas and gets every public header: <SpatialCore/SpatialCore.h> for the
// DSP library and <SpatialCore/SpatialCoreUI.h> for the UI library. The two stay apart on purpose, so
// a DSP-only consumer never pulls JUCE GUI headers through the DSP umbrella.
//
// This file keeps both complete. It reads include/SpatialCore/**/*.h from the source tree (through
// SPATIALCORE_SOURCE_DIR, the same way FontProvenanceTests.cpp reads the font files) and fails
// naming any public header that is in neither umbrella, in the wrong one, or in both.
//
// NOTHING may be included before the next line. Being the first include of this translation unit
// is what proves the DSP umbrella compiles on its own.
#include <SpatialCore/SpatialCore.h>

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace spatialcore;

namespace
{

const std::string kDspUmbrella = "SpatialCore/SpatialCore.h";
const std::string kUiUmbrella  = "SpatialCore/SpatialCoreUI.h";

// Public headers that are deliberately in neither umbrella, each with the reason. Keep this short:
// an entry is an exception to "a consumer gets every public header from an umbrella".
const std::map<std::string, std::string>& allowList()
{
    static const std::map<std::string, std::string> entries {
        { "SpatialCore/Engine/TripleBufferIndex.h",
          "included by Engine/RenderEngine.h; an engine implementation detail, not a consumer entry point" }
    };
    return entries;
}

juce::File includeRoot()
{
    return juce::File (SPATIALCORE_SOURCE_DIR).getChildFile ("include");
}

// The names (SpatialCore/<Area>/<Name>.h) of every .h under include/SpatialCore.
std::set<std::string> publicHeaders()
{
    std::set<std::string> names;
    const auto root = includeRoot();
    for (auto& f : root.getChildFile ("SpatialCore").findChildFiles (juce::File::findFiles, true, "*.h"))
        names.insert (f.getRelativePathFrom (root).replaceCharacter ('\\', '/').toStdString());
    return names;
}

bool startsWith (const std::string& s, const std::string& prefix)
{
    return s.compare (0, prefix.size(), prefix) == 0;
}

// The SpatialCore/... paths named by the `#include <SpatialCore/...>` lines of one umbrella, in file
// order (a header named twice appears twice). Each line is trimmed; only a line that starts with
// `#include <SpatialCore/` counts, so a commented-out include does not.
std::vector<std::string> umbrellaIncludes (const std::string& umbrellaName)
{
    std::vector<std::string> out;
    const auto file = includeRoot().getChildFile (juce::String (umbrellaName));
    if (! file.existsAsFile())
        return out;

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    for (auto line : lines)
    {
        line = line.trim();
        if (! line.startsWith ("#include <SpatialCore/"))
            continue;
        const int close = line.indexOfChar ('>');
        if (close < 0)
            continue;
        out.push_back (line.substring (10, close).toStdString()); // after `#include <`
    }
    return out;
}

int countOf (const std::vector<std::string>& v, const std::string& name)
{
    int n = 0;
    for (auto& s : v)
        if (s == name)
            ++n;
    return n;
}

std::string joined (const std::vector<std::string>& problems)
{
    std::string out;
    for (auto& p : problems)
        out += "\n  " + p;
    return out;
}

} // namespace

TEST_CASE ("Umbrella: every public header is in exactly one umbrella or on the allow-list", "[umbrella]")
{
    const auto headers = publicHeaders();
    REQUIRE (! headers.empty());

    REQUIRE (includeRoot().getChildFile (juce::String (kDspUmbrella)).existsAsFile());
    INFO ("include/" << kUiUmbrella << " must exist (D-02: the UI umbrella)");
    const bool uiUmbrellaExists = includeRoot().getChildFile (juce::String (kUiUmbrella)).existsAsFile();
    CHECK (uiUmbrellaExists);

    const auto dsp = umbrellaIncludes (kDspUmbrella);
    const auto ui  = umbrellaIncludes (kUiUmbrella);

    std::vector<std::string> problems;

    for (auto& h : headers)
    {
        if (h == kDspUmbrella || h == kUiUmbrella)
            continue;

        const int inDsp = countOf (dsp, h);
        const int inUi  = countOf (ui, h);

        if (allowList().count (h) != 0)
        {
            if (inDsp != 0 || inUi != 0)
                problems.push_back (h + " is on the allow-list but an umbrella includes it");
            continue;
        }

        if (startsWith (h, "SpatialCore/UI/"))
        {
            if (inUi != 1)
                problems.push_back (h + " must be included exactly once by " + kUiUmbrella
                                    + " (found " + std::to_string (inUi) + ")");
            if (inDsp != 0)
                problems.push_back (h + " is a UI header and must not be in " + kDspUmbrella + " (D-02)");
        }
        else
        {
            if (inDsp != 1)
                problems.push_back (h + " must be included exactly once by " + kDspUmbrella
                                    + " (found " + std::to_string (inDsp) + ")");
            if (inUi != 0)
                problems.push_back (h + " is not a UI header and must not be in " + kUiUmbrella);
        }
    }

    // Every include line in an umbrella names a header that exists.
    for (auto* umbrella : { &dsp, &ui })
        for (auto& named : *umbrella)
            if (headers.count (named) == 0)
                problems.push_back ("an umbrella includes " + named + ", which does not exist");

    // The DSP umbrella stays UI-free; the UI umbrella holds UI headers only.
    for (auto& named : dsp)
        if (startsWith (named, "SpatialCore/UI/"))
            problems.push_back (kDspUmbrella + " includes " + named + " (D-02: the DSP umbrella is UI-free)");
    for (auto& named : ui)
        if (! startsWith (named, "SpatialCore/UI/"))
            problems.push_back (kUiUmbrella + " includes " + named + " (only SpatialCore/UI/ headers belong there)");

    // Every allow-list entry exists and is not in the DSP umbrella.
    for (auto& entry : allowList())
    {
        if (headers.count (entry.first) == 0)
            problems.push_back ("allow-list entry " + entry.first + " does not exist");
        if (countOf (dsp, entry.first) != 0)
            problems.push_back ("allow-list entry " + entry.first + " is in " + kDspUmbrella);
    }

    INFO ("umbrella problems:" << joined (problems));
    CHECK (problems.empty());
}

TEST_CASE ("Umbrella: the DSP umbrella alone gives a consumer the engine and its opt-in surface", "[umbrella]")
{
    RenderEngine engine;
    engine.prepare (48000.0, 64);
    CHECK (engine.getOversizedBlockCount() == 0);

    CHECK (kAlgorithmIndexVBAP == 5);
    CHECK (kAlwaysEmbeddedHRTFProfile == 5);
    CHECK (static_cast<int> (ADMPositionQuery::kCount_) == 5);

    // Different ports on a loopback host do not conflict; the same port on loopback does.
    CHECK (! oscPortsConflict (9000, 9001, "127.0.0.1"));
    CHECK (oscPortsConflict (9000, 9000, "127.0.0.1"));
}
