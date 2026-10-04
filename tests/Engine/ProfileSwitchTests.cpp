#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include "../Binaural/BinauralMetrics.h"

#include <cstdio>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// HRTF profile switching (DATA-01, click-free swap).
//
// This file starts with the baseline of today's escape-hatch swap. The engine-owned
// switching tests (loader, publish protocol, thread safety, sample-based crossfade) are
// added by Plans 03-06, 03-07 and 03-09.
// ============================================================================

TEST_CASE ("Profile switch: legacy escape-hatch swap KEMAR to SADIE is click-free at 128 and 512 samples (baseline)",
           "[hrtf-switch][click][legacy]")
{
    // The legacy swap is: load into getPrepareRendererIndex()'s renderer, then swapActiveRenderer().
    // It crossfades over kRendererXfadeBlocks (8) BLOCKS, so its length in samples depends on the
    // block size. At 32 and 64 samples it clicks today (worst step / steady step 8.19 and 3.23,
    // 03-RESEARCH.md); those sizes are added by Plan 03-09 together with the fix. 128 and 512 hold
    // (research measured 1.15 and 1.00), so they are the baseline the fix must not regress.
    constexpr double kRate = 48000.0;
    constexpr size_t kTotal = 72000;
    constexpr size_t kSwitchAt = 24000;

    for (const int blockSize : { 128, 512 })
    {
        for (const bool noiseInput : { false, true })
        {
            RenderEngine engine;
            engine.prepare (kRate, blockSize);
            engine.setOutputFormat (OutputFormat::Binaural);
            REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kRate));   // KEMAR

            const std::vector<float> input = noiseInput ? pinkNoise (kTotal, 5, 0.25f)
                                                        : sineWave (kTotal, 440.0, kRate, 0.5f);
            const RenderBlockContext ctx = makeBinauralContext (BinauralPath::HRTF, kRate);

            bool swapped = false;
            size_t switchSample = 0;
            const StereoSignal out = renderThroughEngine (
                engine, ctx, input, { blockSize },
                [] (int64_t) { return Direction { 30.0f, 0.0f }; },
                [&] (int64_t blockStart)
                {
                    if (! swapped && blockStart >= static_cast<int64_t> (kSwitchAt))
                    {
                        loadProfileIntoRenderer (engine.getBinauralRenderer (engine.getPrepareRendererIndex()), 1, kRate);   // SADIE
                        engine.swapActiveRenderer();
                        switchSample = static_cast<size_t> (blockStart);
                        swapped = true;
                    }
                });

            REQUIRE (swapped);
            REQUIRE (signalIsFinite (out));

            const size_t transition = static_cast<size_t> (RenderEngine::kRendererXfadeBlocks) * static_cast<size_t> (blockSize);
            const double stepRatio = switchStepRatio (out, switchSample, transition);
            const double rmsRatio = switchMinRmsRatio (out, switchSample, transition);

            char line[160];
            std::snprintf (line, sizeof (line), "legacy swap blk %d %-5s: step %.3f, min RMS %.3f",
                           blockSize, noiseInput ? "noise" : "sine", stepRatio, rmsRatio);
            WARN (line);

            if (! noiseInput)
            {
                INFO ("block size " << blockSize << " step ratio " << stepRatio);
                CHECK (stepRatio <= 1.5);
            }
            // The noise run only records the level dip; Plan 03-09 owns the dip bound.
        }
    }
}
