#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include "../Support/Watchdog.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// Multi-instance safety (Phase 5, VERIFY-02, D-05). Several engines live in one process when a
// host loads several plugin instances. Two closed OpenSpatialDelay bugs came from that:
//
//   - Spatial-Media-Lab/OpenSpatialDelay#96: an AU and a VST3 instance on one track buzzed and
//     then crashed Reaper. The repo's fix is one HRTFDatabase per BinauralRenderer, so no HRTF
//     data is shared between instances. The first case below pins it as a deterministic
//     invariant: an engine's output must not change because another engine beside it switches
//     profiles (RESEARCH F7).
//   - Spatial-Media-Lab/OpenSpatialDelay#131 (its fix, Spatial-Media-Lab/OpenSpatialDelay#142 (PR), is the process
//     global FFT cache): see SharedFFTCacheTests.cpp, and the eight-engine case at the end of this
//     file.
//
// The proof is in-process and macOS only; it says nothing about a real DAW, Windows or Linux (D-05,
// D-13). Every case runs inside runWithWatchdog, and assertions are made on the test thread only
// (Catch2 3.7.1 assertions are not thread-safe).
// ============================================================================

namespace
{
    constexpr int kObjects = 4;

    /** Drives a small scene (four sines) through one engine block by block, the way a host does.
        Static scenes keep every object where it is; moving scenes drift the azimuths each block. */
    struct SceneRenderer
    {
        RenderEngine& engine;
        RenderBlockContext ctx;
        int blockSize;
        int numOutCh;
        double sampleRate;

        std::vector<float> mono[kObjects];
        std::vector<float> tapFade, distGain;
        std::vector<std::vector<float>> out;
        double phase[kObjects] {};
        long blocksRendered = 0;

        SceneRenderer (RenderEngine& e, int block, double rate, int outChannels)
            : engine (e), blockSize (block), numOutCh (outChannels), sampleRate (rate),
              tapFade (static_cast<size_t> (block), 1.0f),
              distGain (static_cast<size_t> (block), 1.0f),
              out (static_cast<size_t> (outChannels), std::vector<float> (static_cast<size_t> (block), 0.0f))
        {
            for (auto& m : mono)
                m.assign (static_cast<size_t> (block), 0.0f);

            // All three opt-in flags: the engine computes gains, derives its dispatch and picks the
            // binaural path itself, which is how a consumer is meant to drive it.
            ctx.engineComputesGains = true;
            ctx.engineDerivesDispatch = true;
            ctx.engineSelectsHRTF = true;
            ctx.sampleRate = rate;
        }

        /** Renders one block and returns true when every output sample was finite. */
        bool renderOne (bool moving)
        {
            static constexpr float kAzimuth[kObjects] = { 30.0f, -45.0f, 120.0f, -150.0f };
            static constexpr double kFreq[kObjects] = { 220.0, 330.0, 440.0, 550.0 };

            RenderSources sources;
            sources.numSamples = blockSize;

            for (int o = 0; o < kObjects; ++o)
            {
                const double w = 2.0 * 3.14159265358979323846 * kFreq[o] / sampleRate;
                for (auto& v : mono[o])
                {
                    v = 0.2f * static_cast<float> (std::sin (phase[o]));
                    phase[o] += w;
                }

                sources.monoBuffers[o] = mono[o].data();
                sources.tapFadeGainPerSample[o] = tapFade.data();
                sources.distGainPerSample[o] = distGain.data();
                sources.objectLive[o] = true;
                sources.objects[o].enabled = true;
                sources.objects[o].elevationDeg = moving ? 10.0f * static_cast<float> (std::sin (0.01 * static_cast<double> (blocksRendered) + o)) : 0.0f;
                sources.objects[o].distance = 0.5f;
                float az = kAzimuth[o];
                if (moving)
                    az += 0.7f * static_cast<float> (blocksRendered % 500);
                sources.objects[o].azimuthDeg = std::fmod (az + 180.0f, 360.0f) - 180.0f;
            }

            std::vector<float*> ptrs;
            for (auto& channel : out)
                ptrs.push_back (channel.data());

            engine.renderBlock (sources, ctx, ptrs.data(), numOutCh);
            ++blocksRendered;

            bool finite = true;
            for (auto& channel : out)
                for (float v : channel)
                    finite = finite && std::isfinite (v);
            return finite;
        }
    };

    /** Renders `blocks` blocks of the fixed scene and returns the left samples followed by the right
        samples of every block, concatenated block by block. */
    std::vector<float> renderFixed (RenderEngine& engine, int blocks)
    {
        constexpr int kBlock = 256;
        SceneRenderer scene (engine, kBlock, 48000.0, 2);

        std::vector<float> all;
        all.reserve (static_cast<size_t> (blocks) * kBlock * 2);
        for (int b = 0; b < blocks; ++b)
        {
            scene.renderOne (false);
            all.insert (all.end(), scene.out[0].begin(), scene.out[0].end());
            all.insert (all.end(), scene.out[1].begin(), scene.out[1].end());
        }
        return all;
    }

    /** A Binaural engine on profile 2 (CIPIC), loaded and idle, ready to render the fixed scene. */
    std::unique_ptr<RenderEngine> makeProfile2Engine()
    {
        // On the heap: a std::thread has a small stack, and the engine is large.
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (48000.0, 256);
        engine->setOutputFormat (OutputFormat::Binaural);
        engine->setHRTFProfile (2);
        // A timeout here is caught by the caller: it asserts activeProfile == 2 after rendering.
        (void) engine->waitForHRTFProfileIdle (60000 * timeoutScaleFromEnv());
        return engine;
    }

    struct IsolationState
    {
        std::vector<float> alone;
        std::vector<float> beside;
        // WR-05: the profile engine A reports as active after each run. Written inside the
        // watchdog lambda (no Catch assertions there) and asserted on the test thread, so a load
        // that timed out cannot let two Simple-path renders compare equal.
        int aActiveAlone = -1;
        int aActiveBeside = -1;
        std::atomic<bool> bRunning { false };
        std::atomic<bool> bStop { false };
        std::atomic<long> bBlocks { 0 };
        std::atomic<int> bProfileRounds { 0 };
        std::atomic<bool> bNonFinite { false };
    };

    /** Engine B: switches 3, 5, 0, 3 (request, wait idle, render 50 blocks of a moving scene) over
        and over until told to stop, then waits idle so it can be destroyed safely. */
    void runSwitchingEngine (IsolationState& state)
    {
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (48000.0, 256);
        engine->setOutputFormat (OutputFormat::Binaural);

        SceneRenderer scene (*engine, 256, 48000.0, 2);
        const int scale = timeoutScaleFromEnv();
        const int order[] = { 3, 5, 0, 3 };

        // Start from a block already rendered, so "running" means rendering, not preparing.
        scene.renderOne (true);
        state.bRunning.store (true, std::memory_order_release);

        int next = 0;
        while (! state.bStop.load (std::memory_order_acquire))
        {
            engine->setHRTFProfile (order[next++ % 4]);
            engine->waitForHRTFProfileIdle (60000 * scale);

            for (int b = 0; b < 50 && ! state.bStop.load (std::memory_order_acquire); ++b)
            {
                if (! scene.renderOne (true))
                    state.bNonFinite.store (true);
                state.bBlocks.fetch_add (1);
            }
            state.bProfileRounds.fetch_add (1);
        }

        engine->waitForHRTFProfileIdle (60000 * scale);
    }

    /** Count of positions at which two renders differ in any bit. */
    size_t countDifferingSamples (const std::vector<float>& a, const std::vector<float>& b)
    {
        size_t differing = 0;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::memcmp (&a[i], &b[i], sizeof (float)) != 0)
                ++differing;
        return differing;
    }
}

TEST_CASE ("Multi-instance: an engine renders bit-identically whether or not a second engine switches profiles beside it (Spatial-Media-Lab/OpenSpatialDelay#96)",
           "[multi][isolation][tsan]")
{
    auto state = std::make_shared<IsolationState>();

    const bool finished = runWithWatchdog ([state]
    {
        // Run 1: engine A alone.
        {
            auto a1 = makeProfile2Engine();
            state->alone = renderFixed (*a1, 100);
            state->aActiveAlone = a1->getHRTFProfileStatus().activeProfile;
        }

        // Run 2: engine A again, while engine B on its own thread keeps switching profiles.
        {
            std::thread b ([state] { runSwitchingEngine (*state); });

            auto a2 = makeProfile2Engine();
            while (! state->bRunning.load (std::memory_order_acquire))
                std::this_thread::sleep_for (std::chrono::milliseconds (1));

            state->beside = renderFixed (*a2, 100);
            state->aActiveBeside = a2->getHRTFProfileStatus().activeProfile;

            state->bStop.store (true, std::memory_order_release);
            b.join();
        }
    }, std::chrono::milliseconds (180000LL * timeoutScaleFromEnv()));

    REQUIRE (finished);
    // Both runs must have rendered profile 2 (CIPIC), not the Simple path: the idle wait in
    // makeProfile2Engine() returns false on a timeout, and the output would then still be finite
    // and equal run to run.
    REQUIRE (state->aActiveAlone == 2);
    REQUIRE (state->aActiveBeside == 2);
    REQUIRE (state->alone.size() == 51200);
    REQUIRE (state->beside.size() == state->alone.size());

    const size_t differing = countDifferingSamples (state->alone, state->beside);
    INFO ("differing samples: " << differing << " of " << state->alone.size());
    INFO ("engine B rendered " << state->bBlocks.load() << " blocks over " << state->bProfileRounds.load() << " profile rounds");

    const bool silent = std::all_of (state->alone.begin(), state->alone.end(), [] (float v) { return v == 0.0f; });
    CHECK_FALSE (silent);
    CHECK_FALSE (state->bNonFinite.load());
    CHECK (differing == 0);
}

// ----------------------------------------------------------------------------
// Eight engines at once (D-05, VERIFY-02): create, prepare, render, switch and destroy on eight
// threads, twice. Run it in a fresh process (ctest does) so the shared FFT cache is first used
// concurrently. Only the small profiles are used (2 CIPIC, 3 HUTUBS, 5 KEMAR, 1.2 to 1.8 MB):
// eight engines each loading SADIE or Bernschuetz under ThreadSanitizer outlast the engine's own
// 15 s shutdown bound (RESEARCH Pitfall 4). Every engine is waited idle before it is destroyed.
// ----------------------------------------------------------------------------
namespace
{
    constexpr int kStressEngines = 8;
    constexpr int kStressRounds = 2;
    constexpr int kStressBlocks = 400;

    struct StressState
    {
        std::atomic<bool> start { false };
        std::atomic<int> ready { 0 };
        std::atomic<int> finished { 0 };
        long nonFinite[kStressEngines] {};
        long blocks[kStressEngines] {};
    };

    void runStressThread (StressState& state, int index)
    {
        static constexpr int kSmallProfiles[] = { 2, 3, 5 };
        const int scale = timeoutScaleFromEnv();
        const bool surround = (index % 2) != 0;
        const double rate = surround ? 44100.0 : 48000.0;

        state.ready.fetch_add (1);
        while (! state.start.load (std::memory_order_acquire))
            std::this_thread::yield();

        for (int round = 0; round < kStressRounds; ++round)
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (rate, 256);
            engine->setOutputFormat (surround ? OutputFormat::Surround7_1_4 : OutputFormat::Binaural);

            SceneRenderer scene (*engine, 256, rate, surround ? 12 : 2);
            engine->setHRTFProfile (kSmallProfiles[index % 3]);

            for (int b = 0; b < kStressBlocks; ++b)
            {
                if (b == 100)
                    engine->setHRTFProfile (kSmallProfiles[(index + 1) % 3]);
                else if (b == 250)
                    engine->setHRTFProfile (kSmallProfiles[(index + 2) % 3]);

                if (! scene.renderOne (true))
                    ++state.nonFinite[index];
                ++state.blocks[index];
            }

            // The destructor stops the loader with a 15 s bound; let any pending load finish first.
            engine->waitForHRTFProfileIdle (60000 * scale);
        }

        state.finished.fetch_add (1);
    }
}

TEST_CASE ("Multi-instance: eight engines create, prepare, render, switch and destroy on their own threads at once",
           "[multi][stress][tsan]")
{
    auto state = std::make_shared<StressState>();

    const bool finished = runWithWatchdog ([state]
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < kStressEngines; ++i)
            threads.emplace_back ([state, i] { runStressThread (*state, i); });

        while (state->ready.load() < kStressEngines)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        state->start.store (true, std::memory_order_release);

        for (auto& t : threads)
            t.join();
    }, std::chrono::milliseconds (300000LL * timeoutScaleFromEnv()));

    REQUIRE (finished);
    CHECK (state->finished.load() == kStressEngines);

    long totalNonFinite = 0;
    long totalBlocks = 0;
    for (int i = 0; i < kStressEngines; ++i)
    {
        totalNonFinite += state->nonFinite[i];
        totalBlocks += state->blocks[i];
    }

    INFO ("blocks rendered: " << totalBlocks);
    CHECK (totalBlocks == static_cast<long> (kStressEngines) * kStressRounds * kStressBlocks);
    CHECK (totalNonFinite == 0);
}
