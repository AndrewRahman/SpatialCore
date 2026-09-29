#pragma once

#include <SpatialCore/IO/OutputFormat.h>
#include <array>

namespace spatialcore
{

static constexpr int NUM_OUTPUT_FORMATS = 23;

static_assert (NUM_OUTPUT_FORMATS == 23,
    "OutputFormat count changed -- update CLAUDE.md, README.md, "
    "docs/integration-guide.md, .claude/skills/spatialcore-architecture/"
    "spatialcore-architecture.md");

// Output format registry -- single source of truth for all supported formats,
// moved verbatim from OpenSpatialDelayProcessor::outputFormatRegistry.
// Reusable across Spatial Media Library plugins.
class OutputFormatRegistry
{
public:
    static const std::array<OutputFormatInfo, NUM_OUTPUT_FORMATS> table;

    static const OutputFormatInfo& getInfo (OutputFormat format);
    static const char* getDisplayName (OutputFormat format);
};

} // namespace spatialcore
