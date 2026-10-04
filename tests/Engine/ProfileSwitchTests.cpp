#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Binaural/HRTFProfileResolver.h>
#include "../Binaural/BinauralMetrics.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <memory>
#include <random>
#include <thread>
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

TEST_CASE ("Profile switch: legacy escape-hatch swap KEMAR to SADIE is click-free and dropout-free at every block size",
           "[hrtf-switch][click][legacy]")
{
    // The legacy swap is: load into getPrepareRendererIndex()'s renderer, then swapActiveRenderer().
    // The renderer crossfade used to be counted in BLOCKS (8 x the block size), so at 32 and 64
    // samples it was a 5 ms and 11 ms fade and the swap clicked (worst step / steady step 8.19 and
    // 3.23, 03-RESEARCH.md). Plan 03-09 made it sample-based: max (8 x block, 4096) samples. All
    // four sizes are held to the step bound on a sine and to the dropout bound on pink noise.
    constexpr double kRate = 48000.0;
    constexpr size_t kTotal = 72000;
    constexpr size_t kSwitchAt = 24000;

    for (const int blockSize : { 32, 64, 128, 512 })
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

            const size_t transition = std::max (static_cast<size_t> (RenderEngine::kRendererXfadeBlocks) * static_cast<size_t> (blockSize),
                                                static_cast<size_t> (RenderEngine::kMinRendererXfadeSamples));
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
            else
            {
                INFO ("block size " << blockSize << " min RMS ratio " << rmsRatio);
                CHECK (rmsRatio >= 0.7);
            }
        }
    }
}


// ============================================================================
// Engine-owned profile switching (D-04, D-05, D-06): RenderEngine::setHRTFProfile.
// ============================================================================
namespace
{
    /** SPATIALCORE_TEST_TIMEOUT_SCALE: an integer of at least 1 (default 1; anything else counts
        as 1). It stretches watchdogs, idle waits and polls, and the 50 ms bound on a
        setHRTFProfile() call (a wall-clock check that can flake on a loaded machine; at the default
        scale of 1 it is exactly 50 ms, and a load takes far longer than any sensible scale of it).
        The 4.0 sample bound and the 15 s shutdown bound are never scaled. */
    int timeoutScale()
    {
        const char* text = std::getenv ("SPATIALCORE_TEST_TIMEOUT_SCALE");
        if (text == nullptr)
            return 1;

        char* end = nullptr;
        const long value = std::strtol (text, &end, 10);
        if (end == text || *end != '\0' || value < 1 || value > 1000)
            return 1;
        return static_cast<int> (value);
    }

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

        /** Renders until the engine reports no renderer crossfade (at most `maxBlocks`). The fade
            lasts at least kMinRendererXfadeSamples samples, so it spans many small blocks.
            Returns true when it ended. */
        bool renderUntilFadeEnds (int maxBlocks = 800)
        {
            for (int b = 0; b < maxBlocks; ++b)
            {
                renderOne();
                if (! engine.isRendererCrossfadeActive())
                    return true;
            }
            return false;
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

    REQUIRE (engine.waitForHRTFProfileIdle (20000 * timeoutScale()));

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

// ----------------------------------------------------------------------------
// Rules at engine level (D-05, D-06, D-09, D-11, criterion 3)
// ----------------------------------------------------------------------------
namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 64;

    /** Profiles with an embedded copy in this build (all five, or KEMAR only). */
#if SPATIALCORE_EMBEDS_ALL_HRTF
    constexpr bool kAllEmbedded = true;
    const std::vector<int> kEmbeddedProfiles { 1, 2, 3, 4, 5 };
    constexpr int kProbeProfile = 3;            // HUTUBS, IR 279 embedded
    constexpr int kProbeEmbeddedIR = 279;
    constexpr int kOtherProfile = 5;            // any profile other than the probe
#else
    constexpr bool kAllEmbedded = false;
    const std::vector<int> kEmbeddedProfiles { 5 };
    constexpr int kProbeProfile = 5;            // KEMAR, IR 558 embedded
    constexpr int kProbeEmbeddedIR = 558;
    constexpr int kOtherProfile = 0;            // Simple: no other embedded SOFA profile exists
#endif
    constexpr int kDonorIR = 218;               // CIPIC, the real file dropped into the shared folder

    int activeIRLength (RenderEngine& engine)
    {
        return engine.getBinauralRenderer (engine.getActiveRendererIndexAtomic().load()).hrtfDatabase.getIRLength();
    }

    bool activeIsSimple (RenderEngine& engine)
    {
        return engine.getBinauralRenderer (engine.getActiveRendererIndexAtomic().load()).isSimpleMode();
    }

    void prepareBinaural (RenderEngine& engine, const juce::File& sharedFolder)
    {
        engine.prepare (kRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        engine.setSharedHRTFFolderForTesting (sharedFolder);
    }

    /** Switches to `profile`, waits for the worker, then renders until the audio thread has it
        and the crossfade has finished. Returns false at the first thing that does not happen. */
    bool switchAndSettle (RenderEngine& engine, LiveRender& live, int profile)
    {
        engine.setHRTFProfile (profile);
        if (! engine.waitForHRTFProfileIdle (30000 * timeoutScale()))
            return false;
        if (live.renderUntilActive (profile, 400) < 0)
            return false;
        if (! live.renderUntilFadeEnds())
            return false;
        // Profile 0 on the HRTF path is a silent Simple-mode renderer (Plan 03-09 owns that
        // crossfade); only the SOFA profiles are expected to sound here.
        if (profile == 0)
        {
            for (int b = 0; b < 50; ++b)
                live.renderOne();
            return live.lastFinite && ! engine.isRendererCrossfadeActive();
        }
        return live.renderAudible (50) && ! engine.isRendererCrossfadeActive();
    }
}

TEST_CASE ("Profile switch: every embedded profile becomes active through the engine with no shared folder",
           "[hrtf-resolve][embedded][engine]")
{
    // Criterion 3: no file path anywhere, the shared folder is a directory that does not exist.
    const TempFolder nonExistent ("embedded");

    RenderEngine engine;
    prepareBinaural (engine, nonExistent.dir);
    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);

    for (const int profile : kEmbeddedProfiles)
    {
        INFO ("profile " << profile);
        REQUIRE (switchAndSettle (engine, live, profile));

        const HRTFProfileStatus status = engine.getHRTFProfileStatus();
        CHECK (status.state == HRTFLoadState::Ready);
        CHECK (status.source == HRTFProfileSource::Embedded);
        CHECK (status.problem == HRTFProfileProblem::None);
        CHECK (status.activeProfile == profile);
        CHECK_FALSE (activeIsSimple (engine));
        CHECK (live.lastFinite);
        CHECK (live.lastPeak > 1.0e-4f);
    }

    // Profile 0 selects the Simple renderer. On the HRTF path a Simple-mode renderer is silent
    // (the consumer flips useHRTF off); the Simple <-> HRTF crossfade is Plan 03-09.
    engine.setHRTFProfile (0);
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
    REQUIRE (live.renderUntilActive (0, 400) > 0);
    live.renderAudible (20);
    const HRTFProfileStatus status = engine.getHRTFProfileStatus();
    CHECK (status.state == HRTFLoadState::Ready);
    CHECK (status.source == HRTFProfileSource::Simple);
    CHECK (activeIsSimple (engine));
    CHECK (live.lastFinite);
}

TEST_CASE ("Profile switch: a failed request leaves the current profile playing and reports why",
           "[hrtf-switch][failure]")
{
    const TempFolder nonExistent ("failure");

    SECTION ("invalid index while another profile is active")
    {
        RenderEngine engine;
        prepareBinaural (engine, nonExistent.dir);
        LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
        REQUIRE (switchAndSettle (engine, live, kProbeProfile));

        engine.setHRTFProfile (9);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));

        const HRTFProfileStatus status = engine.getHRTFProfileStatus();
        CHECK (status.state == HRTFLoadState::Failed);
        CHECK (status.problem == HRTFProfileProblem::InvalidIndex);
        CHECK (status.requestedProfile == 9);
        CHECK (status.activeProfile == kProbeProfile);
        CHECK (describeHRTFProfileStatus (status).contains ("profile 9 failed"));

        CHECK (live.renderAudible (50));   // never a mute
        CHECK (engine.getHRTFProfileStatus().activeProfile == kProbeProfile);
        CHECK_FALSE (activeIsSimple (engine));
    }

    SECTION ("invalid index on a fresh engine leaves Simple")
    {
        RenderEngine engine;
        prepareBinaural (engine, nonExistent.dir);
        LiveRender live (engine, makeBinauralContext (BinauralPath::Simple, kRate), kBlock, kRate);

        engine.setHRTFProfile (9);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
        live.renderOne();

        const HRTFProfileStatus status = engine.getHRTFProfileStatus();
        CHECK (status.state == HRTFLoadState::Failed);
        CHECK (status.activeProfile == 0);
        CHECK (activeIsSimple (engine));
        CHECK (live.lastFinite);
        CHECK (live.lastPeak > 1.0e-4f);   // the Simple path keeps sounding
    }

    SECTION ("a load that throws settles as failed, never stays Loading, and a retry recovers (WR-02)")
    {
        RenderEngine engine;
        prepareBinaural (engine, nonExistent.dir);
        LiveRender live (engine, makeBinauralContext (BinauralPath::Simple, kRate), kBlock, kRate);

        engine.setLoaderFailureForTesting (true);
        engine.setHRTFProfile (kProbeProfile);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));

        const HRTFProfileStatus status = engine.getHRTFProfileStatus();
        CHECK (status.state == HRTFLoadState::Failed);
        CHECK (status.problem == HRTFProfileProblem::LoadFailed);
        CHECK (status.requestedProfile == kProbeProfile);
        CHECK (status.activeProfile == 0);
        CHECK (describeHRTFProfileStatus (status).contains ("failed"));

        live.renderOne();
        CHECK (live.lastFinite);
        CHECK (live.lastPeak > 1.0e-4f);   // the current profile keeps playing

        // The worker survived the throw: the same request, retried, now loads.
        engine.setLoaderFailureForTesting (false);
        LiveRender hrtfLive (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
        REQUIRE (switchAndSettle (engine, hrtfLive, kProbeProfile));
        CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Ready);
        CHECK (engine.getHRTFProfileStatus().activeProfile == kProbeProfile);
        CHECK_FALSE (activeIsSimple (engine));
    }

    SECTION ("negative index")
    {
        RenderEngine engine;
        prepareBinaural (engine, nonExistent.dir);
        engine.setHRTFProfile (-1);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
        CHECK (engine.getHRTFProfileStatus().problem == HRTFProfileProblem::InvalidIndex);
    }

#if ! SPATIALCORE_EMBEDS_ALL_HRTF
    SECTION ("OFF build: profile 1 with an empty folder is not found")
    {
        const TempFolder empty ("failure-empty", true);
        RenderEngine engine;
        prepareBinaural (engine, empty.dir);
        engine.setHRTFProfile (1);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));

        const HRTFProfileStatus status = engine.getHRTFProfileStatus();
        CHECK (status.state == HRTFLoadState::Failed);
        CHECK (status.problem == HRTFProfileProblem::NotFound);
        CHECK (describeHRTFProfileStatus (status).contains ("profile 1 failed: file missing"));
    }
#endif
}

TEST_CASE ("Profile switch: the most recent request wins, and a settled request is a no-op",
           "[hrtf-switch][latest-wins]")
{
    const TempFolder nonExistent ("latest");

    RenderEngine engine;
    prepareBinaural (engine, nonExistent.dir);

    // Back to back with no audio blocks rendered: each call returns at once.
    const std::vector<int> sequence = kAllEmbedded ? std::vector<int> { 1, 2, 3, 4 } : std::vector<int> { 5, 0, 5, 0 };
    for (const int profile : sequence)
    {
        const auto t0 = std::chrono::steady_clock::now();
        engine.setHRTFProfile (profile);
        INFO ("setHRTFProfile (" << profile << ")");
        CHECK (elapsedMs (t0) < 50.0);
    }
    const int last = sequence.back();

    REQUIRE (engine.waitForHRTFProfileIdle (60000 * timeoutScale()));
    CHECK (engine.getHRTFProfileStatus().requestedProfile == last);

    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
    REQUIRE (live.renderUntilActive (last, 400) > 0);
    CHECK (engine.getHRTFProfileStatus().requestedProfile == last);
    CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Ready);
    REQUIRE (live.renderUntilFadeEnds());
    live.renderAudible (50);

    // The same request again, settled and active: no reload, no crossfade, status unchanged.
    const HRTFProfileStatus before = engine.getHRTFProfileStatus();
    engine.setHRTFProfile (last);
    const HRTFProfileStatus after = engine.getHRTFProfileStatus();
    CHECK (after.state == HRTFLoadState::Ready);
    CHECK (after.source == before.source);
    CHECK (after.activeProfile == before.activeProfile);
    for (int b = 0; b < 10; ++b)
    {
        live.renderOne();
        CHECK_FALSE (engine.isRendererCrossfadeActive());
    }

    // A failed request, requested again, runs again.
    engine.setHRTFProfile (9);
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
    REQUIRE (engine.getHRTFProfileStatus().state == HRTFLoadState::Failed);

    engine.setHRTFProfile (9);
    const HRTFLoadState straightAfter = engine.getHRTFProfileStatus().state;
    CHECK ((straightAfter == HRTFLoadState::Loading || straightAfter == HRTFLoadState::Failed));
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
    CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Failed);
    CHECK (engine.getHRTFProfileStatus().problem == HRTFProfileProblem::InvalidIndex);
    CHECK (engine.getHRTFProfileStatus().activeProfile == last);
}

TEST_CASE ("Profile switch: prepare() during a load is safe and reloads at the new sample rate",
           "[hrtf-switch][prepare]")
{
    const TempFolder nonExistent ("prepare");

    // The reference IR length at 44.1 kHz, and proof that it is not the 48 kHz length.
    HRTFDatabase reference48, reference44;
    constexpr int kProfile = kAllEmbedded ? 1 : 5;
    REQUIRE (reference48.loadFromBinaryData (kProfile, 48000.0f));
    REQUIRE (reference44.loadFromBinaryData (kProfile, 44100.0f));
    REQUIRE (reference44.getIRLength() != reference48.getIRLength());

    RenderEngine engine;
    prepareBinaural (engine, nonExistent.dir);

    engine.setHRTFProfile (kProfile);   // a slow load starts at 48 kHz
    engine.prepare (44100.0, 512);      // must stop it, discard it, and reload at 44.1 kHz

    REQUIRE (engine.waitForHRTFProfileIdle (60000 * timeoutScale()));

    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, 44100.0), kBlock, 44100.0);
    REQUIRE (live.renderUntilActive (kProfile, 400) > 0);
    REQUIRE (live.renderUntilFadeEnds());
    live.renderAudible (50);

    CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Ready);
    CHECK (activeIRLength (engine) == reference44.getIRLength());

    // A second prepare at the same settings must not reload anything.
    engine.prepare (44100.0, 512);
    REQUIRE (engine.waitForHRTFProfileIdle (60000 * timeoutScale()));
    for (int b = 0; b < 10; ++b)
    {
        live.renderOne();
        CHECK_FALSE (engine.isRendererCrossfadeActive());
    }
    CHECK (engine.getHRTFProfileStatus().activeProfile == kProfile);
    CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Ready);
    CHECK (activeIRLength (engine) == reference44.getIRLength());
}

TEST_CASE ("Profile switch: a broken same-name shared file plays the built-in copy and says so",
           "[hrtf-switch][status]")
{
    const TempFolder folder ("status", true);
    const juce::String stub = "version https://git-lfs.github.com/spec/v1\n"
                              "oid sha256:0000000000000000000000000000000000000000000000000000000000000000\n"
                              "size 1658802\n";
    REQUIRE (folder.dir.getChildFile (kHRTFProfiles[kProbeProfile].fileName).replaceWithText (stub));

    RenderEngine engine;
    prepareBinaural (engine, folder.dir);
    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
    REQUIRE (switchAndSettle (engine, live, kProbeProfile));

    const HRTFProfileStatus status = engine.getHRTFProfileStatus();
    CHECK (status.state == HRTFLoadState::Ready);
    CHECK (status.source == HRTFProfileSource::Embedded);
    CHECK (status.problem == HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn);
    CHECK (status.activeProfile == kProbeProfile);
    CHECK (describeHRTFProfileStatus (status).contains ("unreadable, used built-in"));
    CHECK (activeIRLength (engine) == kProbeEmbeddedIR);
}

TEST_CASE ("Profile switch: a file dropped into the shared folder is used on the next switch, no restart",
           "[hrtf-switch][d11]")
{
    const TempFolder folder ("d11", true);   // starts empty

    RenderEngine engine;
    prepareBinaural (engine, folder.dir);
    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);

    REQUIRE (switchAndSettle (engine, live, kProbeProfile));
    CHECK (engine.getHRTFProfileStatus().source == HRTFProfileSource::Embedded);
    CHECK (activeIRLength (engine) == kProbeEmbeddedIR);

    REQUIRE (getSofaFile (testProfileFile (2)).copyFileTo (folder.dir.getChildFile (kHRTFProfiles[kProbeProfile].fileName)));

    REQUIRE (switchAndSettle (engine, live, kOtherProfile));
    REQUIRE (switchAndSettle (engine, live, kProbeProfile));

    const HRTFProfileStatus status = engine.getHRTFProfileStatus();
    CHECK (status.state == HRTFLoadState::Ready);
    CHECK (status.source == HRTFProfileSource::SharedFolder);
    CHECK (status.problem == HRTFProfileProblem::None);
    CHECK (activeIRLength (engine) == kDonorIR);
}

TEST_CASE ("Profile switch: switching while a non-binaural format renders completes and strands nothing",
           "[hrtf-switch][non-binaural]")
{
    const TempFolder nonExistent ("surround");

    RenderEngine engine;
    engine.prepare (kRate, 512);
    engine.setOutputFormat (OutputFormat::Surround5_1);
    engine.setSharedHRTFFolderForTesting (nonExistent.dir);

    RenderBlockContext ctx;
    ctx.engineComputesGains = true;
    ctx.engineDerivesDispatch = true;
    ctx.sampleRate = kRate;
    LiveRender live (engine, ctx, kBlock, kRate, 6);
    live.renderOne();

    engine.setHRTFProfile (5);
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
    const int blocks = live.renderUntilActive (5, 10);
    CHECK (blocks > 0);
    CHECK_FALSE (engine.isRendererCrossfadeActive());
    CHECK (live.lastFinite);

    // The renderer that was active is free again, so a second request completes.
    engine.setHRTFProfile (kProbeProfile == 5 ? 0 : kProbeProfile);
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
    CHECK (live.renderUntilActive (kProbeProfile == 5 ? 0 : kProbeProfile, 10) > 0);
    CHECK_FALSE (engine.isRendererCrossfadeActive());
}

TEST_CASE ("Profile switch: a crossfade abandoned by a path change frees its renderer",
           "[hrtf-switch][abandon]")
{
    const TempFolder nonExistent ("abandon");

    RenderEngine engine;
    prepareBinaural (engine, nonExistent.dir);
    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);

    engine.setHRTFProfile (5);
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
    REQUIRE (live.renderUntilActive (5, 400) > 0);
    REQUIRE (engine.isRendererCrossfadeActive());   // claimed, fading in

    live.ctx = makeBinauralContext (BinauralPath::Simple, kRate);
    live.renderOne();
    CHECK_FALSE (engine.isRendererCrossfadeActive());

    const int next = (kProbeProfile == 5) ? 0 : kProbeProfile;
    engine.setHRTFProfile (next);
    REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));   // needs the abandoned renderer back
    CHECK (live.renderUntilActive (next, 10) > 0);
    CHECK (live.lastFinite);
}

// ============================================================================
// Real concurrency (D-05, T-03-26, T-03-27): a render thread and a switching thread
// running together, and an engine destroyed while its loader is working.
// ============================================================================
namespace
{
    /** Runs `body` on a std::async task and waits seconds x scale. On timeout it prints a named
        message and aborts, so a deadlock ends the test binary with a non-zero exit instead of
        hanging the suite. A plain wait_for timeout would not do: the future returned by
        std::async blocks in its destructor until the task finishes. The body only measures;
        the test asserts on its own thread afterwards (Catch2 assertions stay single-threaded). */
    void runWithWatchdog (const char* name, int seconds, std::function<void()> body)
    {
        const int limit = seconds * timeoutScale();
        std::future<void> done = std::async (std::launch::async, std::move (body));

        if (done.wait_for (std::chrono::seconds (limit)) == std::future_status::timeout)
        {
            std::fprintf (stderr, "WATCHDOG: %s exceeded %d s\n", name, limit);
            std::fflush (stderr);
            std::abort();
        }

        done.get();   // rethrows anything the body threw
    }

    struct ThreadsResult
    {
        bool idleReached = false;
        int finalActive = -1;
        long nonFinite = 0;
        float largestSample = 0.0f;
        long blocksRendered = 0;
        double slowestCallMs = 0.0;
        int callsMade = 0;
        float peakByProfile[6] {};
        long blocksByProfile[6] {};
    };

    struct ShutdownResult
    {
        double destroyMs = -1.0;
    };
}

TEST_CASE ("Profile switch: switching while another thread renders never deadlocks or produces non-finite audio",
           "[hrtf-switch][threads]")
{
    ThreadsResult result;

    runWithWatchdog ("[hrtf-switch][threads]", 60, [&result]
    {
        const TempFolder nonExistent ("threads");   // a path that is never created

        // On the heap: a std::async thread has a small stack, and the engine is large.
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kRate, 512);
        engine->setOutputFormat (OutputFormat::Binaural);
        engine->setSharedHRTFFolderForTesting (nonExistent.dir);

        std::atomic<bool> stop { false };
        std::atomic<long> blocksSoFar { 0 };
        long nonFinite = 0;
        float largest = 0.0f;
        long blocks = 0;
        float peakByProfile[6] {};
        long blocksByProfile[6] {};

        // The render thread stands in for the audio thread: 64-sample blocks of a 440 Hz sine on
        // the HRTF path, azimuth stepping 1 degree per block, as fast as it can go.
        std::thread renderThread ([&]
        {
            LiveRender live (*engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
            int64_t block = 0;
            while (! stop.load (std::memory_order_acquire))
            {
                live.renderOne (static_cast<float> (block % 360) - 180.0f);
                if (! live.lastFinite)
                    ++nonFinite;
                largest = std::max (largest, live.lastPeak);
                const int active = std::clamp (engine->getHRTFProfileStatus().activeProfile, 0, 5);
                peakByProfile[active] = std::max (peakByProfile[active], live.lastPeak);
                ++blocksByProfile[active];
                ++block;
                blocksSoFar.store (static_cast<long> (block), std::memory_order_release);
            }
            blocks = static_cast<long> (block);
        });

        // Let the render thread get going before the first request.
        while (blocksSoFar.load (std::memory_order_acquire) < 50)
            juce::Thread::sleep (1);

        const int order[] = { 5, 3, 0, 4, 2, 5, 3, 0 };
        std::mt19937 rng (0x0307u);
        std::uniform_int_distribution<int> gapMs (20, 120);

        int next = 0;
        for (int call = 0; call < 24; ++call)
        {
            const int profile = (call == 12) ? 1 : order[next++ % 8];

            const auto t0 = std::chrono::steady_clock::now();
            engine->setHRTFProfile (profile);
            result.slowestCallMs = std::max (result.slowestCallMs, elapsedMs (t0));
            ++result.callsMade;

            juce::Thread::sleep (gapMs (rng));
        }

        {
            const auto t0 = std::chrono::steady_clock::now();
            engine->setHRTFProfile (5);
            result.slowestCallMs = std::max (result.slowestCallMs, elapsedMs (t0));
            ++result.callsMade;
        }

        result.idleReached = engine->waitForHRTFProfileIdle (30000 * timeoutScale());

        // The audio thread claims the last result at a block boundary, still rendering.
        const auto pollStart = std::chrono::steady_clock::now();
        while (engine->getHRTFProfileStatus().activeProfile != 5
               && elapsedMs (pollStart) < 10000.0 * timeoutScale())
            juce::Thread::sleep (2);
        result.finalActive = engine->getHRTFProfileStatus().activeProfile;

        stop.store (true, std::memory_order_release);
        renderThread.join();

        result.nonFinite = nonFinite;
        result.largestSample = largest;
        result.blocksRendered = blocks;
        for (int i = 0; i < 6; ++i)
        {
            result.peakByProfile[i] = peakByProfile[i];
            result.blocksByProfile[i] = blocksByProfile[i];
        }
    });

    INFO ("slowest setHRTFProfile call " << result.slowestCallMs << " ms over " << result.callsMade << " calls");
    INFO ("largest sample " << result.largestSample << " over " << result.blocksRendered << " blocks");
    char line[200];
    std::snprintf (line, sizeof (line), "threads: slowest call %.3f ms, largest sample %.4f, %ld blocks, %ld non-finite",
                   result.slowestCallMs, static_cast<double> (result.largestSample), result.blocksRendered, result.nonFinite);
    WARN (line);
    for (int i = 0; i < 6; ++i)
    {
        std::snprintf (line, sizeof (line), "threads: active profile %d: %ld blocks, peak %.4f", i,
                       result.blocksByProfile[i], static_cast<double> (result.peakByProfile[i]));
        WARN (line);
    }

    CHECK (result.callsMade == 25);
    CHECK (result.idleReached);
    CHECK (result.finalActive == 5);
    CHECK (result.nonFinite == 0);
    CHECK (result.largestSample < 4.0f);
    CHECK (result.blocksRendered > 0);
    CHECK (result.slowestCallMs < 50.0 * timeoutScale());
}

TEST_CASE ("Profile switch: every switch completes while a paced render thread runs, claims and frees included",
           "[hrtf-switch][churn]")
{
    // The [threads] case above fires requests faster than a SOFA load finishes, so almost every
    // result is superseded before it is published and only the last switch reaches the audio
    // thread. This case makes every switch complete: it waits for each profile to become active,
    // but not for the crossfade to end, so the next request has to wait for the faded-out renderer
    // to come back free while the render thread keeps rendering at a paced rate (250 us a block).
    struct Result
    {
        int switchesDone = 0;
        int switchesWanted = 0;
        long nonFinite = 0;
        float largestSample = 0.0f;
        float peakByProfile[6] {};
        double slowestCallMs = 0.0;
    } result;

    runWithWatchdog ("[hrtf-switch][churn]", 120, [&result]
    {
        const TempFolder nonExistent ("churn");

        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kRate, 512);
        engine->setOutputFormat (OutputFormat::Binaural);
        engine->setSharedHRTFFolderForTesting (nonExistent.dir);

#if SPATIALCORE_EMBEDS_ALL_HRTF
        const std::vector<int> sequence { 5, 2, 3, 4, 5, 0, 2, 5, 3, 0, 4, 5, 2, 3, 5, 1 };
#else
        const std::vector<int> sequence { 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0 };
#endif
        result.switchesWanted = static_cast<int> (sequence.size());

        std::atomic<bool> stop { false };
        long nonFinite = 0;
        float largest = 0.0f;
        float peakByProfile[6] {};

        std::thread renderThread ([&]
        {
            LiveRender live (*engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
            int64_t block = 0;
            while (! stop.load (std::memory_order_acquire))
            {
                live.renderOne (static_cast<float> (block % 360) - 180.0f);
                if (! live.lastFinite)
                    ++nonFinite;
                largest = std::max (largest, live.lastPeak);
                const int active = std::clamp (engine->getHRTFProfileStatus().activeProfile, 0, 5);
                peakByProfile[active] = std::max (peakByProfile[active], live.lastPeak);
                ++block;
                std::this_thread::sleep_for (std::chrono::microseconds (250));
            }
        });

        std::mt19937 rng (0x0308u);
        std::uniform_int_distribution<int> dwellMs (0, 30);

        for (const int profile : sequence)
        {
            const auto t0 = std::chrono::steady_clock::now();
            engine->setHRTFProfile (profile);
            result.slowestCallMs = std::max (result.slowestCallMs, elapsedMs (t0));

            const auto waitStart = std::chrono::steady_clock::now();
            while (engine->getHRTFProfileStatus().activeProfile != profile
                   && elapsedMs (waitStart) < 20000.0 * timeoutScale())
                juce::Thread::sleep (1);

            if (engine->getHRTFProfileStatus().activeProfile != profile)
                break;

            ++result.switchesDone;
            juce::Thread::sleep (dwellMs (rng));
        }

        stop.store (true, std::memory_order_release);
        renderThread.join();

        result.nonFinite = nonFinite;
        result.largestSample = largest;
        for (int i = 0; i < 6; ++i)
            result.peakByProfile[i] = peakByProfile[i];
    });

    char line[200];
    std::snprintf (line, sizeof (line), "churn: %d of %d switches, slowest call %.3f ms, largest sample %.4f, %ld non-finite",
                   result.switchesDone, result.switchesWanted, result.slowestCallMs,
                   static_cast<double> (result.largestSample), result.nonFinite);
    WARN (line);
    for (int i = 0; i < 6; ++i)
    {
        std::snprintf (line, sizeof (line), "churn: profile %d peak %.4f", i, static_cast<double> (result.peakByProfile[i]));
        WARN (line);
    }

    CHECK (result.switchesDone == result.switchesWanted);
    CHECK (result.nonFinite == 0);
    CHECK (result.largestSample < 4.0f);
    CHECK (result.slowestCallMs < 50.0 * timeoutScale());
    for (int profile = 1; profile <= 5; ++profile)
        if (std::find (kEmbeddedProfiles.begin(), kEmbeddedProfiles.end(), profile) != kEmbeddedProfiles.end())
        {
            INFO ("profile " << profile << " peak " << result.peakByProfile[profile]);
            CHECK (result.peakByProfile[profile] > 1.0e-4f);   // audio kept playing on every SOFA profile
        }
}

TEST_CASE ("Profile switch: destroying the engine during a load returns promptly",
           "[hrtf-switch][shutdown]")
{
    // Section 0 destroys the engine at once. Section 1 waits until the loader has certainly
    // started on the SOFA parse first, which is the case the shutdown contract is about.
    const int delayBeforeDestroyMs = GENERATE (0, 40);

    ShutdownResult result;

    runWithWatchdog ("[hrtf-switch][shutdown]", 15, [&result, delayBeforeDestroyMs]
    {
        const TempFolder nonExistent ("shutdown");

        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kRate, 512);
        engine->setOutputFormat (OutputFormat::Binaural);
        engine->setSharedHRTFFolderForTesting (nonExistent.dir);

        engine->setHRTFProfile (1);   // SADIE, the slowest load
        if (delayBeforeDestroyMs > 0)
            juce::Thread::sleep (delayBeforeDestroyMs);

        const auto t0 = std::chrono::steady_clock::now();
        engine.reset();
        result.destroyMs = elapsedMs (t0);
    });

    INFO ("destroy after " << delayBeforeDestroyMs << " ms took " << result.destroyMs << " ms");
    char line[160];
    std::snprintf (line, sizeof (line), "shutdown: delay %d ms, destruction %.1f ms", delayBeforeDestroyMs, result.destroyMs);
    WARN (line);

    CHECK (result.destroyMs >= 0.0);
    CHECK (result.destroyMs < 15000.0);
}

// ============================================================================
// Click-free and dropout-free switching at every block size (criterion 4, Plan 03-09).
// ============================================================================
namespace
{
    /** One profile switch rendered through the engine: `fromProfile` is settled and playing, then
        `toProfile` is requested after 24000 samples and the engine renders on. */
    struct SwitchRun
    {
        StereoSignal out;
        size_t switchSample = 0;   // first block on which the engine claimed the new profile
        bool ok = false;
    };

    constexpr size_t kSwitchRunTotal = 72000;
    constexpr size_t kSwitchRunAt = 24000;

    /** The fade length the engine promises: max (kRendererXfadeBlocks x block, kMinRendererXfadeSamples). */
    size_t expectedTransition (int blockSize)
    {
        return std::max (static_cast<size_t> (RenderEngine::kRendererXfadeBlocks) * static_cast<size_t> (blockSize),
                         static_cast<size_t> (RenderEngine::kMinRendererXfadeSamples));
    }

    /** Copies the real SOFA file of each listed profile into `dir` under the name the shared-folder
        lookup expects, so a case does not depend on which profiles this build embeds. */
    bool copyProfilesInto (const juce::File& dir, std::initializer_list<int> profiles)
    {
        for (const int profile : profiles)
            if (! getSofaFile (testProfileFile (profile)).copyFileTo (dir.getChildFile (kHRTFProfiles[profile].fileName)))
                return false;
        return true;
    }

    SwitchRun runProfileSwitch (int blockSize, bool noiseInput, int fromProfile, int toProfile,
                                bool engineSelectsHRTF, const juce::File& sharedFolder)
    {
        SwitchRun run;

        RenderEngine engine;
        engine.prepare (kRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        engine.setSharedHRTFFolderForTesting (sharedFolder);

        // Simple (0) is what a fresh engine plays; every other profile is loaded by the worker.
        if (fromProfile != 0)
        {
            engine.setHRTFProfile (fromProfile);
            if (! engine.waitForHRTFProfileIdle (30000 * timeoutScale()))
                return run;
        }

        RenderBlockContext ctx = makeBinauralContext (BinauralPath::HRTF, kRate);
        ctx.engineSelectsHRTF = engineSelectsHRTF;

        const std::vector<float> input = noiseInput ? pinkNoise (kSwitchRunTotal, 5, 0.25f)
                                                    : sineWave (kSwitchRunTotal, 440.0, kRate, 0.5f);

        bool issued = false;
        bool detected = false;
        int64_t lastBlockStart = 0;

        run.out = renderThroughEngine (
            engine, ctx, input, { blockSize },
            [] (int64_t) { return Direction { 30.0f, 0.0f }; },
            [&] (int64_t blockStart)
            {
                // The claim happens at the top of the first block after the request has settled,
                // so the first block after which the crossfade is active (or the new profile is
                // active) is the block rendered just before this callback.
                if (issued && ! detected
                    && (engine.isRendererCrossfadeActive() || engine.getHRTFProfileStatus().activeProfile == toProfile))
                {
                    run.switchSample = static_cast<size_t> (lastBlockStart);
                    detected = true;
                }
                lastBlockStart = blockStart;

                if (! issued && blockStart >= static_cast<int64_t> (kSwitchRunAt))
                {
                    engine.setHRTFProfile (toProfile);
                    engine.waitForHRTFProfileIdle (30000 * timeoutScale());
                    issued = true;
                }
            });

        run.ok = issued && detected && signalIsFinite (run.out);
        return run;
    }

    struct SwitchMetrics
    {
        double stepRatio = 0.0;
        double noiseRmsRatio = 0.0;
        double sineRmsRatio = 0.0;
        bool ok = false;
    };

    SwitchMetrics measureProfileSwitch (int blockSize, int fromProfile, int toProfile,
                                        bool engineSelectsHRTF, const juce::File& sharedFolder)
    {
        SwitchMetrics m;
        const size_t transition = expectedTransition (blockSize);

        const SwitchRun sine = runProfileSwitch (blockSize, false, fromProfile, toProfile, engineSelectsHRTF, sharedFolder);
        const SwitchRun noise = runProfileSwitch (blockSize, true, fromProfile, toProfile, engineSelectsHRTF, sharedFolder);
        m.ok = sine.ok && noise.ok;
        if (! m.ok)
            return m;

        m.stepRatio = switchStepRatio (sine.out, sine.switchSample, transition);
        m.sineRmsRatio = switchMinRmsRatio (sine.out, sine.switchSample, transition);
        m.noiseRmsRatio = switchMinRmsRatio (noise.out, noise.switchSample, transition);
        return m;
    }

    void reportSwitch (const char* label, int blockSize, int fromProfile, int toProfile, const SwitchMetrics& m)
    {
        char line[200];
        std::snprintf (line, sizeof (line), "%s %d -> %d blk %3d: sine step %.3f, noise RMS %.3f, sine RMS %.3f",
                       label, fromProfile, toProfile, blockSize, m.stepRatio, m.noiseRmsRatio, m.sineRmsRatio);
        WARN (line);
    }
}

TEST_CASE ("Profile switch: setHRTFProfile KEMAR to SADIE is click-free and dropout-free at every block size",
           "[hrtf-switch][click]")
{
    // SADIE (1) is copied into a temporary shared folder, so the case also runs in a build that
    // embeds only KEMAR. KEMAR (5) is embedded in every build.
    const TempFolder folder ("click", true);
    REQUIRE (copyProfilesInto (folder.dir, { 1 }));

    for (const int blockSize : { 32, 64, 128, 512 })
    {
        INFO ("block size " << blockSize);
        const SwitchMetrics m = measureProfileSwitch (blockSize, 5, 1, false, folder.dir);
        REQUIRE (m.ok);
        reportSwitch ("setHRTFProfile", blockSize, 5, 1, m);

        CHECK (m.stepRatio <= 1.5);        // the sine: steps are meaningful
        CHECK (m.noiseRmsRatio >= 0.7);    // the pink noise: a dropout is meaningful
    }
}

TEST_CASE ("Profile switch: the renderer crossfade lasts max (8 blocks, 4096 samples) on both swap paths",
           "[hrtf-switch][click][xfade-length]")
{
    // The length is fixed when the fade starts and counted in elapsed samples, so at 512-sample
    // blocks and above it is the old 8-block fade (4096 samples) and below that it is 4096 samples.
    const TempFolder folder ("xfade-length", true);
    REQUIRE (copyProfilesInto (folder.dir, { 1 }));   // SADIE, so a KEMAR-only build can load it too

    // Counts the blocks a fade runs for: the first block on which the engine reports a crossfade
    // is the claim block, and the block on which it stops reporting is the last one.
    const auto fadeBlocks = [] (LiveRender& live) -> int
    {
        int blocks = 0;
        for (int b = 0; b < 2000; ++b)
        {
            live.renderOne();
            if (live.engine.isRendererCrossfadeActive())
                ++blocks;
            else if (blocks > 0)
                return blocks + 1;
        }
        return -1;
    };

    for (const int blockSize : { 32, 64, 128, 512 })
    {
        for (const bool legacy : { false, true })
        {
            INFO ("block size " << blockSize << (legacy ? " legacy swap" : " setHRTFProfile"));

            RenderEngine engine;
            engine.prepare (kRate, 512);
            engine.setOutputFormat (OutputFormat::Binaural);
            engine.setSharedHRTFFolderForTesting (folder.dir);
            LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), blockSize, kRate);

            // Settle on KEMAR first (this runs the Simple -> KEMAR fade to its end).
            REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kRate));
            for (int b = 0; b < 400; ++b)
                live.renderOne();
            REQUIRE_FALSE (engine.isRendererCrossfadeActive());

            if (legacy)
            {
                REQUIRE (loadProfileIntoRenderer (engine.getBinauralRenderer (engine.getPrepareRendererIndex()), 1, kRate));
                engine.swapActiveRenderer();
            }
            else
            {
                engine.setHRTFProfile (1);
                REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
            }

            const int blocks = fadeBlocks (live);
            REQUIRE (blocks > 0);
            CHECK (static_cast<size_t> (blocks) * static_cast<size_t> (blockSize) == expectedTransition (blockSize));
        }
    }
}

// ============================================================================
// engineSelectsHRTF (D-15): the engine picks the render path itself and crossfades
// Simple and HRTF. Plan 03-09, Task 2.
// ============================================================================
namespace
{
    /** Appends both channels of the block `live` just rendered. */
    void appendBlock (const LiveRender& live, std::vector<float>& left, std::vector<float>& right)
    {
        left.insert (left.end(), live.out[0].begin(), live.out[0].end());
        right.insert (right.end(), live.out[1].begin(), live.out[1].end());
    }

    /** Number of samples that differ (compared with ==) and the first one, for the failure message. */
    size_t countMismatches (const std::vector<float>& a, const std::vector<float>& b, size_t& first)
    {
        size_t count = 0;
        first = 0;
        if (a.size() != b.size())
            return std::max (a.size(), b.size());
        for (size_t i = 0; i < a.size(); ++i)
            if (! (a[i] == b[i]))
            {
                if (count == 0)
                    first = i;
                ++count;
            }
        return count;
    }

    RenderBlockContext selectsHRTFContext (bool consumerUseHRTF)
    {
        RenderBlockContext ctx = makeBinauralContext (BinauralPath::HRTF, kRate);
        ctx.engineSelectsHRTF = true;
        ctx.useHRTF = consumerUseHRTF;   // ignored by the engine; the tests set it wrong on purpose
        return ctx;
    }

    void prepareLegacy (RenderEngine& engine)
    {
        engine.prepare (kRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
    }
}

TEST_CASE ("Profile switch: with engineSelectsHRTF a switch between Simple and an HRTF profile is click-free and dropout-free",
           "[hrtf-switch][simple]")
{
    // The real files go into a temporary shared folder, so every profile works in every build.
    const TempFolder folder ("simple", true);
    REQUIRE (copyProfilesInto (folder.dir, { 3, 5 }));

    for (const int profile : { 5, 3 })
    {
        for (const int blockSize : { 32, 64, 128, 512 })
        {
            INFO ("profile " << profile << " block size " << blockSize);

            const SwitchMetrics toHRTF = measureProfileSwitch (blockSize, 0, profile, true, folder.dir);
            REQUIRE (toHRTF.ok);
            reportSwitch ("engineSelectsHRTF", blockSize, 0, profile, toHRTF);
            CHECK (toHRTF.stepRatio <= 1.5);
            CHECK (toHRTF.noiseRmsRatio >= 0.7);

            const SwitchMetrics fromHRTF = measureProfileSwitch (blockSize, profile, 0, true, folder.dir);
            REQUIRE (fromHRTF.ok);
            reportSwitch ("engineSelectsHRTF", blockSize, profile, 0, fromHRTF);
            CHECK (fromHRTF.stepRatio <= 1.5);
            CHECK (fromHRTF.noiseRmsRatio >= 0.7);
        }
    }
}

TEST_CASE ("Profile switch: with engineSelectsHRTF Simple to and from each of profiles 1, 2 and 4 is click-free at 32 samples",
           "[hrtf-switch][simple]")
{
    const TempFolder folder ("simple-rest", true);
    REQUIRE (copyProfilesInto (folder.dir, { 1, 2, 4 }));

    for (const int profile : { 1, 2, 4 })
    {
        INFO ("profile " << profile);

        const SwitchMetrics toHRTF = measureProfileSwitch (32, 0, profile, true, folder.dir);
        REQUIRE (toHRTF.ok);
        reportSwitch ("engineSelectsHRTF", 32, 0, profile, toHRTF);
        CHECK (toHRTF.stepRatio <= 1.5);
        CHECK (toHRTF.noiseRmsRatio >= 0.7);

        const SwitchMetrics fromHRTF = measureProfileSwitch (32, profile, 0, true, folder.dir);
        REQUIRE (fromHRTF.ok);
        reportSwitch ("engineSelectsHRTF", 32, profile, 0, fromHRTF);
        CHECK (fromHRTF.stepRatio <= 1.5);
        CHECK (fromHRTF.noiseRmsRatio >= 0.7);
    }
}

TEST_CASE ("Profile switch: during a Simple <-> HRTF fade the Woodworth path runs once per block and the two are blended equal-power",
           "[hrtf-switch][simple][blend]")
{
    // The blend is rebuilt outside the engine: the Simple side from a flag-off Simple engine, the HRTF
    // side from a flag-off HRTF engine, and the ramp from the renderer crossfade's published law
    // (cos / sin of the end-of-block progress, linear across the block, length 4096 samples). If the
    // Woodworth path ran twice per block its cue filters and gain interpolation would diverge from the
    // flag-off Simple engine and the match would fail. A source at 150 degrees azimuth drives the
    // rear cue so the filter state matters.
    constexpr int kB = 64;
    constexpr int kLead = 10;
    constexpr float kAz = 150.0f;
    constexpr int kFadeBlocks = RenderEngine::kMinRendererXfadeSamples / kB;
    constexpr int kTail = 8;
    constexpr int kTotal = kLead + kFadeBlocks + kTail;
    constexpr double kPhasePerSample = 2.0 * 3.14159265358979323846 * 440.0 / kRate;

    for (const bool towardsHRTF : { true, false })
    {
        INFO ("direction " << (towardsHRTF ? "Simple -> HRTF" : "HRTF -> Simple"));

        std::vector<float> aL, aR;           // the engine under test
        {
            RenderEngine engine;
            prepareLegacy (engine);
            if (! towardsHRTF)
                REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kRate));
            LiveRender live (engine, selectsHRTFContext (true), kB, kRate);
            for (int b = 0; b < kLead; ++b)
            {
                live.renderOne (kAz);
                appendBlock (live, aL, aR);
            }
            REQUIRE (loadProfileIntoRenderer (engine.getBinauralRenderer (engine.getPrepareRendererIndex()),
                                              towardsHRTF ? 5 : 0, kRate));
            engine.swapActiveRenderer();
            for (int b = kLead; b < kTotal; ++b)
            {
                live.renderOne (kAz);
                appendBlock (live, aL, aR);
            }
        }

        // Reference signals: `oldSide` runs the whole time, `newSide` starts at the swap block.
        std::vector<float> oldL, oldR, newL, newR;
        {
            RenderEngine engine;
            prepareLegacy (engine);
            if (! towardsHRTF)
                REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kRate));
            LiveRender live (engine, makeBinauralContext (towardsHRTF ? BinauralPath::Simple : BinauralPath::HRTF, kRate),
                             kB, kRate);
            for (int b = 0; b < kTotal; ++b)
            {
                live.renderOne (kAz);
                appendBlock (live, oldL, oldR);
            }
        }
        {
            RenderEngine engine;
            prepareLegacy (engine);
            if (towardsHRTF)
                REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kRate));
            LiveRender live (engine, makeBinauralContext (towardsHRTF ? BinauralPath::HRTF : BinauralPath::Simple, kRate),
                             kB, kRate);
            live.phase = static_cast<double> (kLead * kB) * kPhasePerSample;
            for (int b = kLead; b < kTotal; ++b)
            {
                live.renderOne (kAz);
                appendBlock (live, newL, newR);
            }
        }

        REQUIRE (aL.size() == static_cast<size_t> (kTotal * kB));
        REQUIRE (oldL.size() == aL.size());
        REQUIRE (newL.size() == static_cast<size_t> ((kTotal - kLead) * kB));

        // Before the swap the engine is the plain path it replaces, bit for bit.
        size_t first = 0;
        const std::vector<float> aLeadL (aL.begin(), aL.begin() + kLead * kB);
        const std::vector<float> oldLeadL (oldL.begin(), oldL.begin() + kLead * kB);
        CHECK (countMismatches (aLeadL, oldLeadL, first) == 0);

        // From the swap on: out = old * cos + new * sin, ramped linearly across each block.
        constexpr float halfPi = juce::MathConstants<float>::halfPi;
        float gOutPrev = 1.0f, gInPrev = 0.0f;
        double worst = 0.0;
        int samplesDone = 0;
        for (int b = 0; b < kTotal - kLead; ++b)
        {
            samplesDone += kB;
            float progress = static_cast<float> (static_cast<double> (samplesDone) / static_cast<double> (RenderEngine::kMinRendererXfadeSamples));
            progress = std::min (progress, 1.0f);
            const float gOutEnd = std::cos (progress * halfPi);
            const float gInEnd = std::sin (progress * halfPi);
            const float outInc = (gOutEnd - gOutPrev) / static_cast<float> (kB);
            const float inInc = (gInEnd - gInPrev) / static_cast<float> (kB);
            float gOut = gOutPrev, gIn = gInPrev;
            const bool fading = samplesDone - kB < RenderEngine::kMinRendererXfadeSamples;

            for (int i = 0; i < kB; ++i)
            {
                const size_t n = static_cast<size_t> ((kLead + b) * kB + i);
                const size_t m = static_cast<size_t> (b * kB + i);
                float expectL, expectR;
                if (fading)
                {
                    gOut += outInc;
                    gIn += inInc;
                    expectL = oldL[n] * gOut + newL[m] * gIn;
                    expectR = oldR[n] * gOut + newR[m] * gIn;
                }
                else
                {
                    expectL = newL[m];
                    expectR = newR[m];
                }
                worst = std::max ({ worst, static_cast<double> (std::abs (aL[n] - expectL)),
                                    static_cast<double> (std::abs (aR[n] - expectR)) });
            }
            if (fading)
            {
                gOutPrev = gOutEnd;
                gInPrev = gInEnd;
            }
        }

        // Not two silences: the engine output and both rebuilt sides carry real signal.
        CHECK (*std::max_element (aL.begin() + kLead * kB, aL.end()) > 1.0e-3f);
        CHECK (*std::max_element (oldL.begin() + kLead * kB, oldL.end()) > 1.0e-3f);
        CHECK (*std::max_element (newL.begin(), newL.end()) > 1.0e-3f);

        char line[160];
        std::snprintf (line, sizeof (line), "blend %s: worst deviation from the rebuilt blend %.3g",
                       towardsHRTF ? "Simple -> HRTF" : "HRTF -> Simple", worst);
        WARN (line);
        CHECK (worst < 2.0e-6);
    }
}

TEST_CASE ("Profile switch: engineSelectsHRTF with no switch is bit-identical to the path the active renderer implies",
           "[hrtf-switch][flag-identity]")
{
    constexpr int kB = 64;
    constexpr int kBlocks = 20;

    SECTION ("Simple active equals the flag-off Simple path")
    {
        RenderEngine flagOn, flagOff;
        prepareLegacy (flagOn);
        prepareLegacy (flagOff);
        LiveRender onLive (flagOn, selectsHRTFContext (true), kB, kRate);    // the consumer's useHRTF is wrong
        LiveRender offLive (flagOff, makeBinauralContext (BinauralPath::Simple, kRate), kB, kRate);

        std::vector<float> onL, onR, offL, offR;
        for (int b = 0; b < kBlocks; ++b)
        {
            const float az = 20.0f + 9.0f * static_cast<float> (b);
            onLive.renderOne (az);
            offLive.renderOne (az);
            appendBlock (onLive, onL, onR);
            appendBlock (offLive, offL, offR);
        }

        size_t first = 0;
        CHECK (countMismatches (onL, offL, first) == 0);
        CHECK (countMismatches (onR, offR, first) == 0);
        CHECK (*std::max_element (onL.begin(), onL.end()) > 1.0e-4f);   // not comparing two silences
    }

    SECTION ("HRTF active equals the flag-off useHRTF = true path")
    {
        RenderEngine flagOn, flagOff;
        prepareLegacy (flagOn);
        prepareLegacy (flagOff);
        REQUIRE (loadProfileIntoActiveRenderer (flagOn, 5, kRate));
        REQUIRE (loadProfileIntoActiveRenderer (flagOff, 5, kRate));
        LiveRender onLive (flagOn, selectsHRTFContext (false), kB, kRate);   // wrong: HRTF is active
        LiveRender offLive (flagOff, makeBinauralContext (BinauralPath::HRTF, kRate), kB, kRate);

        std::vector<float> onL, onR, offL, offR;
        for (int b = 0; b < kBlocks; ++b)
        {
            const float az = 20.0f + 9.0f * static_cast<float> (b);
            onLive.renderOne (az);
            offLive.renderOne (az);
            appendBlock (onLive, onL, onR);
            appendBlock (offLive, offL, offR);
        }

        size_t first = 0;
        CHECK (countMismatches (onL, offL, first) == 0);
        CHECK (countMismatches (onR, offR, first) == 0);
        CHECK (*std::max_element (onL.begin(), onL.end()) > 1.0e-4f);
    }

    SECTION ("a switch between two HRTF profiles equals the same switch with the flag off")
    {
        RenderEngine flagOn, flagOff;
        prepareLegacy (flagOn);
        prepareLegacy (flagOff);
        LiveRender onLive (flagOn, selectsHRTFContext (false), kB, kRate);
        LiveRender offLive (flagOff, makeBinauralContext (BinauralPath::HRTF, kRate), kB, kRate);

        for (RenderEngine* engine : { &flagOn, &flagOff })
            REQUIRE (loadProfileIntoActiveRenderer (*engine, 5, kRate));

        std::vector<float> onL, onR, offL, offR;
        const auto renderBoth = [&] (int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                onLive.renderOne (40.0f);
                offLive.renderOne (40.0f);
                appendBlock (onLive, onL, onR);
                appendBlock (offLive, offL, offR);
            }
        };

        renderBoth (10);
        for (RenderEngine* engine : { &flagOn, &flagOff })
        {
            REQUIRE (loadProfileIntoRenderer (engine->getBinauralRenderer (engine->getPrepareRendererIndex()), 1, kRate));
            engine->swapActiveRenderer();
        }
        renderBoth (RenderEngine::kMinRendererXfadeSamples / kB + 8);   // the whole fade and a little more

        size_t first = 0;
        CHECK (countMismatches (onL, offL, first) == 0);
        CHECK (countMismatches (onR, offR, first) == 0);
    }
}

TEST_CASE ("Profile switch: engineSelectsHRTF alone computes the Woodworth gains, so profile 0 is never silent (WR-06)",
           "[hrtf-switch][flag-identity][wr06]")
{
    constexpr int kB = 64;
    constexpr int kBlocks = 20;

    RenderEngine selectsOnly, selectsAndComputes;
    prepareLegacy (selectsOnly);
    prepareLegacy (selectsAndComputes);

    RenderBlockContext onlyCtx = selectsHRTFContext (false);
    onlyCtx.engineComputesGains = false;                       // the footgun: objGains stay zero unless the engine fills them
    RenderBlockContext bothCtx = selectsHRTFContext (false);   // engineComputesGains is set by makeBinauralContext
    REQUIRE (bothCtx.engineComputesGains);

    LiveRender onlyLive (selectsOnly, onlyCtx, kB, kRate);
    LiveRender bothLive (selectsAndComputes, bothCtx, kB, kRate);

    std::vector<float> onlyL, onlyR, bothL, bothR;
    for (int b = 0; b < kBlocks; ++b)
    {
        const float az = 20.0f + 9.0f * static_cast<float> (b);
        onlyLive.renderOne (az);
        bothLive.renderOne (az);
        appendBlock (onlyLive, onlyL, onlyR);
        appendBlock (bothLive, bothL, bothR);
    }

    size_t first = 0;
    CHECK (*std::max_element (onlyL.begin(), onlyL.end()) > 1.0e-4f);   // not silence
    CHECK (countMismatches (onlyL, bothL, first) == 0);                 // the same sound as both flags set
    CHECK (countMismatches (onlyR, bothR, first) == 0);
}

TEST_CASE ("Profile switch: engineSelectsHRTF ignores the consumer's useHRTF on every block, through a switch",
           "[hrtf-switch][flag-identity]")
{
    constexpr int kB = 64;
    const TempFolder nonExistent ("ignore");

    std::vector<float> lL, lR;   // consumer value always false
    std::vector<float> mL, mR;   // consumer value changes pseudo-randomly every block

    for (const bool varying : { false, true })
    {
        RenderEngine engine;
        prepareBinaural (engine, nonExistent.dir);
        LiveRender live (engine, selectsHRTFContext (false), kB, kRate);
        std::mt19937 rng (0x0309u);
        auto& left = varying ? mL : lL;
        auto& right = varying ? mR : lR;

        const auto renderBlocks = [&] (int count)
        {
            for (int b = 0; b < count; ++b)
            {
                live.ctx.useHRTF = varying ? ((rng() & 1u) != 0) : false;
                live.renderOne (35.0f);
                appendBlock (live, left, right);
            }
        };

        renderBlocks (10);                                  // Simple
        engine.setHRTFProfile (kProbeProfile);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
        renderBlocks (RenderEngine::kMinRendererXfadeSamples / kB + 20);   // Simple -> HRTF, then HRTF
        engine.setHRTFProfile (0);
        REQUIRE (engine.waitForHRTFProfileIdle (30000 * timeoutScale()));
        renderBlocks (RenderEngine::kMinRendererXfadeSamples / kB + 20);   // HRTF -> Simple, then Simple
    }

    size_t first = 0;
    CHECK (countMismatches (lL, mL, first) == 0);
    CHECK (countMismatches (lR, mR, first) == 0);
    CHECK (*std::max_element (lL.begin(), lL.end()) > 1.0e-4f);
}
