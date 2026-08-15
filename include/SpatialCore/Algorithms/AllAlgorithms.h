#pragma once

// Spatial Media Library -- all spatialization algorithm headers
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Algorithms/ConstantPowerAlgorithm.h>

#include <type_traits>

namespace spatialcore
{

/** Compile-time list of the concrete SpatializationAlgorithm implementations this
    header exposes, excluding the abstract SpatializationAlgorithm base.

    NUM_ALGORITHMS is DERIVED from this list rather than written as a literal, so the
    count cannot silently disagree with the list it claims to count (D-04). Every entry
    is checked to actually derive from SpatializationAlgorithm, so the list cannot be
    padded with a type that is not an algorithm.

    Types only -- no storage, no allocation, nothing reachable from processBlock. */
template <typename... Algorithms>
struct AlgorithmTypeList
{
    static_assert ((std::is_base_of_v<SpatializationAlgorithm, Algorithms> && ...),
        "Every entry in AllAlgorithmTypes must derive from SpatializationAlgorithm");

    static constexpr int size = static_cast<int> (sizeof...(Algorithms));
};

using AllAlgorithmTypes = AlgorithmTypeList<
    ConstantPowerAlgorithm,
    VBAPAlgorithm,
    VBIPAlgorithm,
    KNNAlgorithm,
    DBAPAlgorithm,
    MDAPAlgorithm,
    AmbisonicsAlgorithm,
    DirectBinauralAlgorithm>;

// Derived, not hand-written: adding an algorithm to AllAlgorithmTypes changes this.
static constexpr int NUM_ALGORITHMS = AllAlgorithmTypes::size;

// Tripwire. Because NUM_ALGORITHMS is derived above, extending AllAlgorithmTypes
// genuinely fires this assert and forces the doc surfaces to be updated with it.
static_assert (NUM_ALGORITHMS == 8,
    "Algorithm count changed -- update CLAUDE.md, README.md, "
    "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
    "spatialcore-architecture.md");

} // spatialcore
