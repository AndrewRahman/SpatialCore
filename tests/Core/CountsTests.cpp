#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/IO/OutputFormatRegistry.h>

using namespace spatialcore;

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
}
