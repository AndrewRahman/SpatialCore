#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include "../Support/RealtimeGuard.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// Realtime safety (Phase 5, RTSF-05). A trap library (tests/Support/rtguard) counts every
// allocation, free and pthread lock made on the thread it is armed on; these cases arm it around
// RenderEngine::renderBlock and expect zero of each.
//
// Discipline (DR-1, RESEARCH Pitfalls 1 and 3):
//   - every input buffer is filled, and every message-side call made, while the trap is DISARMED;
//     it is armed only around exactly one renderBlock call;
//   - arm from the FIRST block after prepare(). A warm-up hides an allocation that only happens
//     while a distance is still moving, so there is none;
//   - each cold-start combination is its own TEST_CASE. ctest runs every case in its own process, so
//     process-global lazy state is cold there. Running the executable directly shares one process,
//     which is fine for the warm cases but not for a cold-start proof.
//   - every trap case starts with requireRealtimeTrapCanary(): a trap that cannot see reports a
//     false zero. Where no trap is linked (non-Apple, or a sanitizer tree) the cases SKIP.
//
// MovingScene, OutputBank and armedRender are reused and extended by Plans 05-02 and 05-03.
// ============================================================================

namespace
{
    constexpr int kBlockSize = 256;
    constexpr int kBlockCount = 120;
    constexpr double kSampleRate = 48000.0;

    float wrapDegrees (float deg)
    {
        while (deg > 180.0f)  deg -= 360.0f;
        while (deg < -180.0f) deg += 360.0f;
        return deg;
    }

    // Twelve live objects whose azimuth, elevation and distance change every block, so the
    // smoothed Ambisonics NFC distance moves by more than 0.01 m every block.
    struct MovingScene
    {
        static constexpr int kMaxBlock = 4096;

        explicit MovingScene (double rate = kSampleRate)
            : sampleRate (rate)
        {
            for (int t = 0; t < MAX_SOURCES; ++t)
            {
                mono[(size_t) t].assign ((size_t) kMaxBlock, 0.0f);
                tapFade[(size_t) t].assign ((size_t) kMaxBlock, 1.0f);
                distGain[(size_t) t].assign ((size_t) kMaxBlock, 1.0f);
            }
        }

        // Writes the inputs for one block. Call while the trap is disarmed.
        void fill (int blockIndex, int numSamples)
        {
            const double twoPi = 6.283185307179586;

            for (int t = 0; t < MAX_SOURCES; ++t)
            {
                const double freq = 220.0 + 20.0 * t;
                auto& m = mono[(size_t) t];

                for (int i = 0; i < numSamples; ++i)
                {
                    m[(size_t) i] = 0.25f * (float) std::sin (phase[(size_t) t]);
                    phase[(size_t) t] += twoPi * freq / sampleRate;

                    if (phase[(size_t) t] > twoPi)
                        phase[(size_t) t] -= twoPi;
                }

                const float b = (float) blockIndex;
                const float ft = (float) t;
                objects[(size_t) t].azimuthDeg = wrapDegrees (-165.0f + 30.0f * ft + 5.0f * b);
                objects[(size_t) t].elevationDeg = 60.0f * std::sin (0.2f * b + 0.5f * ft);
                objects[(size_t) t].distance = 0.5f + 0.4f * std::sin (0.3f * b + ft);
                objects[(size_t) t].enabled = true;
            }
        }

        // Every object live and enabled.
        RenderSources sources (int numSamples) const
        {
            RenderSources s;
            s.numSamples = numSamples;

            for (int t = 0; t < MAX_SOURCES; ++t)
            {
                s.monoBuffers[t] = mono[(size_t) t].data();
                s.tapFadeGainPerSample[t] = tapFade[(size_t) t].data();
                s.distGainPerSample[t] = distGain[(size_t) t].data();
                s.objects[t] = objects[(size_t) t];
                s.objectLive[t] = true;
                s.sourceEnabledBlock[t] = true;
            }

            return s;
        }

        double sampleRate;
        std::array<std::vector<float>, MAX_SOURCES> mono;
        std::array<std::vector<float>, MAX_SOURCES> tapFade;
        std::array<std::vector<float>, MAX_SOURCES> distGain;
        std::array<ObjectState, MAX_SOURCES> objects {};
        std::array<double, MAX_SOURCES> phase {};
    };

    // Output channel storage for up to 64 channels (49 are enough for 6th-order Ambisonics).
    struct OutputBank
    {
        static constexpr int kMaxChannels = 64;

        OutputBank()
        {
            channels.assign ((size_t) kMaxChannels, std::vector<float> ((size_t) MovingScene::kMaxBlock, 0.0f));

            for (int c = 0; c < kMaxChannels; ++c)
                ptrs[c] = channels[(size_t) c].data();
        }

        void clear()
        {
            for (auto& ch : channels)
                std::fill (ch.begin(), ch.end(), 0.0f);
        }

        bool allFinite (int numCh, int n) const
        {
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (channels[(size_t) c][(size_t) i]))
                        return false;

            return true;
        }

        std::vector<std::vector<float>> channels;
        float* ptrs[kMaxChannels] = {};
    };

    // Renders `blocks` blocks of `blockSize` samples from a cold start, the trap armed around each
    // renderBlock call only. Returns the counts accumulated over every block.
    RealtimeCounts armedRender (RenderEngine& engine, MovingScene& scene, const RenderBlockContext& ctx,
                                OutputBank& out, int numOutCh, int blocks, int blockSize,
                                bool& allFiniteOut)
    {
        allFiniteOut = true;
        resetRealtimeCounts();

        for (int block = 0; block < blocks; ++block)
        {
            scene.fill (block, blockSize);   // disarmed
            out.clear();                     // disarmed
            const RenderSources sources = scene.sources (blockSize);

            {
                const ScopedRealtimeTrap trap;
                engine.renderBlock (sources, ctx, out.ptrs, numOutCh);
            }

            if (! out.allFinite (numOutCh, blockSize))
                allFiniteOut = false;
        }

        const RealtimeCounts counts = realtimeCounts();

        if (counts.allocs != 0)
            printFirstOffender();

        return counts;
    }

    RenderBlockContext makeEngineContext()
    {
        RenderBlockContext ctx;
        ctx.sampleRate = kSampleRate;
        ctx.engineComputesGains = true;
        ctx.engineDerivesDispatch = true;
        return ctx;
    }

    int channelsFor (OutputFormat format)
    {
        return OutputFormatRegistry::getInfo (format).requiredChannels;
    }
}

TEST_CASE ("Realtime: the trap sees a deliberate allocation and a deliberate lock",
           "[rt][rt-alloc][rt-lock][canary]")
{
    requireRealtimeTrapCanary();
}

TEST_CASE ("Realtime: a cold-start 7.1.4 render of 12 moving objects allocates and locks nothing",
           "[rt][rt-alloc][rt-lock][tracer]")
{
    requireRealtimeTrapCanary();

    auto engine = std::make_unique<RenderEngine>();
    engine->prepare (kSampleRate, kBlockSize);
    engine->setOutputFormat (OutputFormat::Surround7_1_4);

    const RenderBlockContext ctx = makeEngineContext();
    const int numOutCh = channelsFor (OutputFormat::Surround7_1_4);

    MovingScene scene;
    OutputBank out;
    bool finite = false;
    const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, numOutCh,
                                               kBlockCount, kBlockSize, finite);

    CAPTURE (counts.allocs, counts.frees, counts.locks);
    CHECK (counts.allocs == 0);
    CHECK (counts.frees == 0);
    CHECK (counts.locks == 0);
    CHECK (finite);
}
