#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include "../Binaural/BinauralMetrics.h"

#include <chrono>
#include <cmath>
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


// ============================================================================
// Engine-owned profile switching (D-04, D-05, D-06): RenderEngine::setHRTFProfile.
// ============================================================================
namespace
{
    /** A unique folder path under the system temp directory, deleted with the object. The
        folder is created only when `create` is true, so a default fixture is a path that
        does not exist (the shared folder the engine must fall back from). */
    struct TempFolder
    {
        juce::File dir;

        explicit TempFolder (const juce::String& tag, bool create = false)
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("sc-switch-" + tag + "-"
                                      + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64())))
        {
            if (create)
                dir.createDirectory();
        }

        ~TempFolder() { dir.deleteRecursively(); }
    };

    /** Drives one live source (a 440 Hz sine) through an engine block by block, so a test can
        interleave requests with rendering the way a host does. */
    struct LiveRender
    {
        RenderEngine& engine;
        RenderBlockContext ctx;
        int blockSize;
        int numOutCh;
        double sampleRate;

        std::vector<float> mono, tapFade, distGain;
        std::vector<std::vector<float>> out;
        double phase = 0.0;

        float lastPeak = 0.0f;
        bool lastFinite = true;

        LiveRender (RenderEngine& e, const RenderBlockContext& c, int block, double rate, int outChannels = 2)
            : engine (e), ctx (c), blockSize (block), numOutCh (outChannels), sampleRate (rate),
              mono (static_cast<size_t> (block), 0.0f),
              tapFade (static_cast<size_t> (block), 1.0f),
              distGain (static_cast<size_t> (block), 1.0f),
              out (static_cast<size_t> (outChannels), std::vector<float> (static_cast<size_t> (block), 0.0f))
        {
        }

        void renderOne (float azDeg = 30.0f)
        {
            const double w = 2.0 * 3.14159265358979323846 * 440.0 / sampleRate;
            for (auto& v : mono)
            {
                v = 0.5f * static_cast<float> (std::sin (phase));
                phase += w;
            }

            RenderSources sources;
            sources.numSamples = blockSize;
            sources.monoBuffers[0] = mono.data();
            sources.tapFadeGainPerSample[0] = tapFade.data();
            sources.distGainPerSample[0] = distGain.data();
            sources.objectLive[0] = true;
            sources.objects[0].azimuthDeg = azDeg;
            sources.objects[0].elevationDeg = 0.0f;
            sources.objects[0].distance = 0.5f;
            sources.objects[0].enabled = true;

            std::vector<float*> ptrs;
            for (auto& channel : out)
                ptrs.push_back (channel.data());

            engine.renderBlock (sources, ctx, ptrs.data(), numOutCh);

            lastPeak = 0.0f;
            lastFinite = true;
            for (auto& channel : out)
                for (float v : channel)
                {
                    lastFinite = lastFinite && std::isfinite (v);
                    lastPeak = std::max (lastPeak, std::abs (v));
                }
        }

        /** Renders blocks until the engine reports `profile` as the active one. Returns the
            number of blocks rendered, or -1 when `maxBlocks` ran out first. */
        int renderUntilActive (int profile, int maxBlocks)
        {
            for (int b = 0; b < maxBlocks; ++b)
            {
                renderOne();
                if (engine.getHRTFProfileStatus().activeProfile == profile)
                    return b + 1;
            }
            return -1;
        }

        /** Renders `count` blocks and reports whether every one was finite and the last 8 had a
            non-silent peak. */
        bool renderAudible (int count)
        {
            bool ok = true;
            for (int b = 0; b < count; ++b)
            {
                renderOne();
                ok = ok && lastFinite;
                if (b >= count - 8)
                    ok = ok && lastPeak > 1.0e-4f;
            }
            return ok;
        }
    };

    double elapsedMs (std::chrono::steady_clock::time_point since)
    {
        return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - since).count();
    }
}

TEST_CASE ("Profile switch: one call loads KEMAR from embedded data and the audio thread plays it",
           "[hrtf-switch][tracer]")
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 64;

    const TempFolder nonExistent ("tracer");   // a path that is never created

    RenderEngine engine;
    engine.prepare (kRate, 512);
    engine.setOutputFormat (OutputFormat::Binaural);
    engine.setSharedHRTFFolderForTesting (nonExistent.dir);

    const auto t0 = std::chrono::steady_clock::now();
    engine.setHRTFProfile (5);
    const double callMs = elapsedMs (t0);
    INFO ("setHRTFProfile took " << callMs << " ms");
    CHECK (callMs < 50.0);

    REQUIRE (engine.waitForHRTFProfileIdle (20000));

    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
    REQUIRE (live.renderUntilActive (5, 400) > 0);

    const HRTFProfileStatus status = engine.getHRTFProfileStatus();
    CHECK (status.state == HRTFLoadState::Ready);
    CHECK (status.source == HRTFProfileSource::Embedded);
    CHECK (status.problem == HRTFProfileProblem::None);
    CHECK (status.requestedProfile == 5);

    CHECK (live.renderAudible (50));
    CHECK_FALSE (engine.getBinauralRenderer (engine.getActiveRendererIndexAtomic().load()).isSimpleMode());
}
