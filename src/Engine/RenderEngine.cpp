#include <SpatialCore/Engine/RenderEngine.h>

// NOTE: This is the Task 1 (header/contract + inventory) stub body. The real
// verbatim-transplanted render path bodies land in Task 2 (08-06 Plan,
// "Transplant the 5 render paths + format-switching into the engine"). Task 1
// is interface-first ordering only, per the plan's own task split — this
// file exists solely so the SpatialCore CMake target configures/links with
// the new Engine/RenderEngine.h contract in place before the transplant.

namespace spatialcore
{

RenderEngine::RenderEngine() = default;
RenderEngine::~RenderEngine() = default;

void RenderEngine::prepare (double /*sampleRate*/, int /*maxBlockSize*/)
{
    // Task 2 fills this in (lfeFilter.prepare/reset + coefficient set, plus
    // sizing sourceAccumStorage_/wetBufL_/wetBufR_).
}

void RenderEngine::renderBlock (const RenderSources& /*sources*/,
                                 const RenderBlockContext& /*blockCtx*/,
                                 float* const* /*outChannels*/,
                                 int /*numOutCh*/)
{
    // Task 2 transplants the 5-branch dispatch here.
    jassertfalse; // not yet implemented — Task 1 stub
}

void RenderEngine::setOutputFormat (OutputFormat format)
{
    activateLayout (format);
}

OutputFormat RenderEngine::getActiveOutputFormat() const
{
    return getActiveLayout().format;
}

const RenderEngine::LayoutState& RenderEngine::getActiveLayout() const
{
    return layoutBuffers[activeLayoutIndex.load (std::memory_order_acquire)];
}

void RenderEngine::activateLayout (OutputFormat /*format*/)
{
    // Task 2 transplants the verbatim activateLayout() body (the
    // double-buffered atomic-swap machinery moved from OSD's
    // OutputLayoutState/activateLayout, per the locked IO-ownership
    // decision).
    jassertfalse; // not yet implemented — Task 1 stub
}

void RenderEngine::computeAmbiDecodeForLayout (const SpeakerLayout& /*layout*/,
                                                float (* /*outMatrix*/)[MAX_SPEAKERS],
                                                int& /*outNumSpeakers*/)
{
    // Task 2 transplants the verbatim computeAmbiDecodeForLayout() body.
    jassertfalse; // not yet implemented — Task 1 stub
}

void RenderEngine::renderDirectBinauralHRTF (const RenderSources& /*sources*/,
                                              float* /*outL*/, float* /*outR*/, int /*numOutCh*/)
{
    jassertfalse; // Task 2
}

void RenderEngine::renderSimpleBinauralWoodworth (const RenderSources& /*sources*/,
                                                    const RenderBlockContext& /*blockCtx*/,
                                                    float* /*outL*/, float* /*outR*/, int /*numOutCh*/)
{
    jassertfalse; // Task 2
}

void RenderEngine::renderStereoVariant (const RenderSources& /*sources*/,
                                          const RenderBlockContext& /*blockCtx*/,
                                          float* /*outL*/, float* /*outR*/, int /*numOutCh*/)
{
    jassertfalse; // Task 2
}

void RenderEngine::renderAmbisonicsOutput (const RenderSources& /*sources*/,
                                             const RenderBlockContext& /*blockCtx*/,
                                             float* const* /*outChannels*/, int /*numOutCh*/)
{
    jassertfalse; // Task 2
}

void RenderEngine::renderDiscreteSurround (const RenderSources& /*sources*/,
                                             const RenderBlockContext& /*blockCtx*/,
                                             float* const* /*outChannels*/, int /*numOutCh*/)
{
    jassertfalse; // Task 2
}

} // namespace spatialcore
