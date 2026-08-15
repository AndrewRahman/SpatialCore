#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace spatialcore;

namespace
{
/** Instantiate one of every algorithm named in AllAlgorithmTypes. The pack is
    deduced from the header's own list, so this cannot drift from it -- there is no
    second hand-written list to forget. Allocation is fine here: test scope only,
    never reachable from processBlock. */
template <typename... Algorithms>
std::vector<std::unique_ptr<SpatializationAlgorithm>>
instantiateAll (AlgorithmTypeList<Algorithms...>)
{
    std::vector<std::unique_ptr<SpatializationAlgorithm>> out;
    out.reserve (sizeof...(Algorithms));
    (out.emplace_back (std::make_unique<Algorithms>()), ...);
    return out;
}
} // namespace

// ============================================================================
// [counts] -- runtime backstop for the frozen public count contract (D-04,
// D-13). Each header carries a compile-time static_assert pinning the same
// number; these TEST_CASEs are the DR-4 runtime double-check that reads the
// enum/registry directly rather than re-stating the literal, so a mismatch
// between the sentinel and the actual enum body still turns the suite red.
// ============================================================================

TEST_CASE ("counts: OutputFormat enum and registry agree on 23 formats", "[counts]")
{
    CHECK (NUM_OUTPUT_FORMATS == 23);
    CHECK (static_cast<int> (OutputFormat::Ambisonics6OA) == NUM_OUTPUT_FORMATS - 1);

    // The registry half of the claim this test's name makes: the table must carry
    // exactly one row per format, in enum order, with no gaps or duplicates.
    CHECK (static_cast<int> (OutputFormatRegistry::table.size()) == NUM_OUTPUT_FORMATS);
    for (int i = 0; i < NUM_OUTPUT_FORMATS; ++i)
        CHECK (static_cast<int> (OutputFormatRegistry::table[static_cast<size_t> (i)].format) == i);
}

TEST_CASE ("counts: LayoutID sentinel pins 15 speaker layouts", "[counts]")
{
    CHECK (NUM_LAYOUT_DEFS == 15);
}

TEST_CASE ("counts: NUM_ALGORITHMS matches the real algorithm list", "[counts]")
{
    CHECK (NUM_ALGORITHMS == 8);

    // Bind the count to real implementations rather than restating the literal:
    // instantiate every type in AllAlgorithmTypes and confirm we get 8 live,
    // distinctly-named SpatializationAlgorithms.
    const auto algorithms = instantiateAll (AllAlgorithmTypes{});
    REQUIRE (static_cast<int> (algorithms.size()) == NUM_ALGORITHMS);

    std::set<std::string> names;
    for (const auto& algorithm : algorithms)
    {
        REQUIRE (algorithm != nullptr);
        names.insert (algorithm->getName().toStdString());
    }

    CHECK (static_cast<int> (names.size()) == NUM_ALGORITHMS);
    CHECK (names.count (std::string()) == 0);
}
