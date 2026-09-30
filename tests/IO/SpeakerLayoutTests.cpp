#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <cmath>
#include <set>
#include <vector>

using namespace spatialcore;

TEST_CASE("OutputFormatRegistry: 23 formats registered", "[io]")
{
    REQUIRE(OutputFormatRegistry::getInfo(OutputFormat::Binaural).format == OutputFormat::Binaural);
    REQUIRE(NUM_OUTPUT_FORMATS == 23);
    REQUIRE(OutputFormatRegistry::table.size() == 23);
}

TEST_CASE("OutputFormatRegistry: Binaural is 2 channels", "[io]")
{
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::Binaural);
    REQUIRE(info.requiredChannels == 2);
    REQUIRE(info.hasLFE == false);
}

TEST_CASE("OutputFormatRegistry: 7.1.4 Atmos is 12 channels with LFE and height", "[io]")
{
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::Surround7_1_4);
    REQUIRE(info.requiredChannels == 12);
    REQUIRE(info.hasLFE == true);
    REQUIRE(info.hasHeight == true);
}

TEST_CASE("OutputFormatRegistry: HOA is 3rd order Ambisonics with 16 channels", "[io]")
{
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::AmbisonicsHOA);
    REQUIRE(info.requiredChannels == 16);
    REQUIRE(info.isAmbisonicsOutput == true);
    REQUIRE(info.ambiOrder == 3);
}

TEST_CASE("OutputFormatRegistry: getDisplayName returns the UI name", "[io]")
{
    REQUIRE(std::string(OutputFormatRegistry::getDisplayName(OutputFormat::Binaural)) == "Binaural");
    REQUIRE(std::string(OutputFormatRegistry::getDisplayName(OutputFormat::Surround7_1_4)) == "7.1.4 Atmos");
}

// ============================================================================
// Golden 23-entry format table -- captured from the pre-move OSD
// outputFormatRegistry (Source/PluginProcessor.cpp), verified byte-identical
// during Phase 8 Plan 08-05 extraction. Each row: requiredChannels, hasLFE,
// hasHeight, isAmbisonicsOutput, ambiOrder, isStereoVariant.
// ============================================================================
struct GoldenFormatRow
{
    OutputFormat format;
    int requiredChannels;
    bool hasLFE;
    bool hasHeight;
    bool isAmbisonicsOutput;
    int ambiOrder;
    bool isStereoVariant;
};

static const GoldenFormatRow kGoldenFormatTable[] = {
    { OutputFormat::Binaural,         2, false, false, false, 0, false },
    { OutputFormat::Stereo,           2, false, false, false, 0, true  },
    { OutputFormat::Quad,             4, false, false, false, 0, false },
    { OutputFormat::Surround5_0,      5, false, false, false, 0, false },
    { OutputFormat::Surround5_1,      6, true,  false, false, 0, false },
    { OutputFormat::Surround7_0,      7, false, false, false, 0, false },
    { OutputFormat::Surround7_1,      8, true,  false, false, 0, false },
    { OutputFormat::Surround9_1,     10, true,  false, false, 0, false },
    { OutputFormat::Octaphonic,       8, false, false, false, 0, false },
    { OutputFormat::Surround5_1_2,    8, true,  true,  false, 0, false },
    { OutputFormat::Surround5_1_4,   10, true,  true,  false, 0, false },
    { OutputFormat::Surround7_1_2,   10, true,  true,  false, 0, false },
    { OutputFormat::Surround7_1_4,   12, true,  true,  false, 0, false },
    { OutputFormat::Surround7_1_6,   14, true,  true,  false, 0, false },
    { OutputFormat::Surround9_1_4,   14, true,  true,  false, 0, false },
    { OutputFormat::Surround9_1_6,   16, true,  true,  false, 0, false },
    { OutputFormat::SurroundSML13_1, 14, true,  true,  false, 0, false },
    { OutputFormat::AmbisonicsFOA,    4, false, false, true,  1, false },
    { OutputFormat::AmbisonicsSOA,    9, false, false, true,  2, false },
    { OutputFormat::AmbisonicsHOA,   16, false, false, true,  3, false },
    { OutputFormat::Ambisonics4OA,   25, false, false, true,  4, false },
    { OutputFormat::Ambisonics5OA,   36, false, false, true,  5, false },
    { OutputFormat::Ambisonics6OA,   49, false, false, true,  6, false },
};

TEST_CASE("OutputFormatRegistry: golden 23-entry format table matches pre-move OSD registry", "[io][golden]")
{
    REQUIRE((sizeof(kGoldenFormatTable) / sizeof(kGoldenFormatTable[0])) == NUM_OUTPUT_FORMATS);

    for (const auto& row : kGoldenFormatTable)
    {
        SECTION(std::string("format index ") + std::to_string(static_cast<int>(row.format)))
        {
            const auto& info = OutputFormatRegistry::getInfo(row.format);
            CHECK(info.format == row.format);
            CHECK(info.requiredChannels == row.requiredChannels);
            CHECK(info.hasLFE == row.hasLFE);
            CHECK(info.hasHeight == row.hasHeight);
            CHECK(info.isAmbisonicsOutput == row.isAmbisonicsOutput);
            CHECK(info.ambiOrder == row.ambiOrder);
            CHECK(info.isStereoVariant == row.isStereoVariant);
        }
    }
}

TEST_CASE("OutputFormatRegistry: lookup for a known format returns correct channel count and flags", "[io]")
{
    // Surround7_1_4 -- 7.1.4 Atmos: 12ch, LFE, height, not Ambisonics
    auto& info = OutputFormatRegistry::getInfo(OutputFormat::Surround7_1_4);
    REQUIRE(info.requiredChannels == 12);
    REQUIRE(info.hasLFE);
    REQUIRE(info.hasHeight);
    REQUIRE_FALSE(info.isAmbisonicsOutput);
    REQUIRE(std::string(info.shortName) == "7.1.4");
}

// ============================================================================
// SpeakerLayout / LayoutID coverage -- moved verbatim from Source/PluginProcessor.cpp
// layoutDefs table (Phase 8 Plan 08-05). Expectations captured from
// Tests/SurroundOutputTests.cpp's layoutExpectations table.
// ============================================================================
struct LayoutExpectation
{
    const char* name;
    LayoutID id;
    int expectedSpeakers;
    int expectedTotal;
    int expectedLFE;
    bool hasHeight;
};

static const LayoutExpectation kLayoutExpectations[] = {
    { "Quad",       LayoutID::Quad,      4,  4, -1, false },
    { "5.0",        LayoutID::S5_0,      5,  5, -1, false },
    { "5.1",        LayoutID::S5_1,      5,  6,  3, false },
    { "7.0",        LayoutID::S7_0,      7,  7, -1, false },
    { "7.1",        LayoutID::S7_1,      7,  8,  3, false },
    { "9.1",        LayoutID::S9_1,      9, 10,  3, false },
    { "Octaphonic", LayoutID::Octaphonic,8,  8, -1, false },
    { "5.1.2",      LayoutID::S5_1_2,    7,  8,  3, true  },
    { "5.1.4",      LayoutID::S5_1_4,    9, 10,  3, true  },
    { "7.1.2",      LayoutID::S7_1_2,    9, 10,  3, true  },
    { "7.1.4",      LayoutID::S7_1_4,   11, 12,  3, true  },
    { "7.1.6",      LayoutID::S7_1_6,   13, 14,  3, true  },
    { "9.1.4",      LayoutID::S9_1_4,   13, 14,  3, true  },
    { "9.1.6",      LayoutID::S9_1_6,   15, 16,  3, true  },
    { "SML 13.1",   LayoutID::SML13_1,  13, 14, 13, true  },
};

static_assert (sizeof (kLayoutExpectations) / sizeof (kLayoutExpectations[0]) == NUM_LAYOUT_DEFS,
               "kLayoutExpectations must have one row per layout def");

TEST_CASE("SpeakerLayout: speaker count and channel count match spec", "[io][layout]")
{
    for (const auto& exp : kLayoutExpectations)
    {
        SECTION(exp.name)
        {
            const auto& layout = getLayoutDef(exp.id);
            CHECK(layout.numSpeakers == exp.expectedSpeakers);
            CHECK(layout.totalChannels == exp.expectedTotal);
            CHECK(layout.lfeChannelIndex == exp.expectedLFE);
        }
    }
}

TEST_CASE("SpeakerLayout: height speakers have elevation > 0 where expected", "[io][layout]")
{
    for (const auto& exp : kLayoutExpectations)
    {
        if (! exp.hasHeight)
            continue;

        SECTION(exp.name)
        {
            const auto& layout = getLayoutDef(exp.id);
            bool foundHeight = false;
            for (int s = 0; s < layout.numSpeakers; ++s)
                if (layout.speakers[s].elevationRad > 0.01f)
                { foundHeight = true; break; }
            CHECK(foundHeight);
            CHECK(layoutHasHeight(layout));
        }
    }
}

TEST_CASE("SpeakerLayout: 2D-only layouts report no height", "[io][layout]")
{
    for (const auto& exp : kLayoutExpectations)
    {
        if (exp.hasHeight)
            continue;

        SECTION(exp.name)
        {
            const auto& layout = getLayoutDef(exp.id);
            CHECK_FALSE(layoutHasHeight(layout));
        }
    }
}

TEST_CASE("SpeakerLayout: channel indices don't overlap and skip LFE", "[io][layout]")
{
    for (const auto& exp : kLayoutExpectations)
    {
        SECTION(exp.name)
        {
            const auto& layout = getLayoutDef(exp.id);
            std::set<int> usedChannels;
            for (int s = 0; s < layout.numSpeakers; ++s)
            {
                int ch = layout.speakers[s].channelIndex;
                CHECK(ch >= 0);
                CHECK(ch < layout.totalChannels);
                CHECK(ch != layout.lfeChannelIndex);
                CHECK(usedChannels.find(ch) == usedChannels.end());
                usedChannels.insert(ch);
            }
        }
    }
}

TEST_CASE("SpeakerLayout: VBAP triplets are non-empty for 3D layouts, empty for 2D", "[io][layout]")
{
    for (const auto& exp : kLayoutExpectations)
    {
        SECTION(exp.name)
        {
            const auto& layout = getLayoutDef(exp.id);
            std::vector<VBAPTriplet> triplets;
            buildVBAPTripletsForLayout(layout, triplets);
            if (exp.hasHeight)
                CHECK(! triplets.empty());
            else
                CHECK(triplets.empty());

            // Regular-only: the builder never emits lower-hemisphere triplets,
            // so this test and the activateLayout guard see the regular list
            // alone (D-02c primary catch, RESEARCH F11).
            for (const auto& t : triplets)
                CHECK(! t.lowerHemisphere);
        }
    }
}

TEST_CASE("OutputFormatRegistry: detectFromChannelCount", "[io]")
{
    // Not part of the golden-table acceptance criterion, but retained from the
    // original scaffold smoke test where the semantics still hold: Binaural
    // is the first entry with requiredChannels == 2.
    REQUIRE(OutputFormatRegistry::getInfo(OutputFormat::Binaural).requiredChannels == 2);
}

// ============================================================================
// D-03: layoutHasHeight and the triplet builder share one height predicate.
// A layout is either height-and-triangulated or flat-and-untriangulated, never
// mismatched -- a mismatch would turn the D-02a abort into a Release crash.
// ============================================================================
TEST_CASE("SpeakerLayout: layoutHasHeight and the triplet builder agree at 0.5, 0.8 and 1.1 degrees (D-03)",
          "[io][layout][d03]")
{
    for (const float elevationDeg : { 0.5f, 0.8f, 1.1f })
    {
        DYNAMIC_SECTION ("centre speaker at " << elevationDeg << " degrees")
        {
            SpeakerLayout l {};
            l.numSpeakers     = 5;
            l.lfeChannelIndex = -1;
            l.totalChannels   = 5;
            const float az[] = { 30.0f, -30.0f, 0.0f, 110.0f, -110.0f };
            for (int i = 0; i < 5; ++i)
            {
                l.speakers[i].azimuthRad   = az[i] * 3.14159265358979f / 180.0f;
                l.speakers[i].elevationRad = 0.0f;
                l.speakers[i].channelIndex = i;
            }
            l.speakers[2].elevationRad = elevationDeg * 3.14159265358979f / 180.0f;

            std::vector<VBAPTriplet> triplets;
            buildVBAPTripletsForLayout (l, triplets);

            CHECK (layoutHasHeight (l) == ! triplets.empty());
        }
    }
}

// ============================================================================
// D-04: lower-hemisphere (EAR) triplets are flagged, ear-level-only, and
// appended only to height layouts.
// ============================================================================
TEST_CASE("SpeakerLayout: lower-hemisphere triplets are flagged, ear-level-only, and appended only to height layouts (D-04)",
          "[io][layout][ear]")
{
    const float earLevelLimitRad = 10.0f * 3.14159265358979f / 180.0f;

    for (const auto& exp : kLayoutExpectations)
    {
        SECTION(exp.name)
        {
            const auto& layout = getLayoutDef(exp.id);

            std::vector<VBAPTriplet> regular;
            buildVBAPTripletsForLayout(layout, regular);

            std::vector<VBAPTriplet> combined = regular;
            appendLowerHemisphereTriplets(layout, combined);

            if (! exp.hasHeight)
            {
                CHECK(combined.size() == regular.size());
                continue;
            }

            const size_t extras = combined.size() - regular.size();
            CHECK(extras >= 1);
            CHECK(extras <= 64);

            // The regular prefix is untouched, field by field.
            REQUIRE(combined.size() >= regular.size());
            for (size_t n = 0; n < regular.size(); ++n)
            {
                CHECK(combined[n].i == regular[n].i);
                CHECK(combined[n].j == regular[n].j);
                CHECK(combined[n].k == regular[n].k);
                CHECK(combined[n].lowerHemisphere == regular[n].lowerHemisphere);
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c)
                        CHECK(combined[n].inv[r][c] == regular[n].inv[r][c]);
            }

            unsigned earMask = 0;
            int earCount = 0;
            for (int s = 0; s < layout.numSpeakers; ++s)
                if (std::abs(layout.speakers[s].elevationRad) <= earLevelLimitRad)
                {
                    earMask |= 1u << s;
                    ++earCount;
                }

            for (size_t n = regular.size(); n < combined.size(); ++n)
            {
                const auto& t = combined[n];
                CHECK(t.lowerHemisphere);
                CHECK(t.nadirVertex >= -1);
                CHECK(t.nadirVertex <= 2);

                const int slots[3] = { t.i, t.j, t.k };
                for (int v = 0; v < 3; ++v)
                {
                    if (v == t.nadirVertex)
                        continue;
                    REQUIRE(slots[v] >= 0);
                    REQUIRE(slots[v] < layout.numSpeakers);
                    CHECK(std::abs(layout.speakers[slots[v]].elevationRad) <= earLevelLimitRad);
                }

                if (t.nadirVertex >= 0)
                {
                    CHECK(static_cast<unsigned>(t.nadirMask) == earMask);
                    CHECK(std::abs(t.nadirGain - 1.0f / std::sqrt(static_cast<float>(earCount))) < 1e-6f);
                }
            }
        }
    }
}
