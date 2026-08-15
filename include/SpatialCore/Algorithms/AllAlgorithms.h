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

namespace spatialcore
{

// Number of concrete SpatializationAlgorithm implementations in this header,
// excluding the abstract SpatializationAlgorithm base.
static constexpr int NUM_ALGORITHMS = 8;

static_assert (NUM_ALGORITHMS == 8,
    "Algorithm count changed -- update CLAUDE.md, README.md, "
    "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
    "spatialcore-architecture.md");

} // spatialcore
