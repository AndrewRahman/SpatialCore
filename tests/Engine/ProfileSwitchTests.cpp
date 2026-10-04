#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Binaural/HRTFProfileResolver.h>
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
        if (! engine.waitForHRTFProfileIdle (30000))
            return false;
        if (live.renderUntilActive (profile, 400) < 0)
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
    REQUIRE (engine.waitForHRTFProfileIdle (30000));
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
        REQUIRE (engine.waitForHRTFProfileIdle (30000));

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
        REQUIRE (engine.waitForHRTFProfileIdle (30000));
        live.renderOne();

        const HRTFProfileStatus status = engine.getHRTFProfileStatus();
        CHECK (status.state == HRTFLoadState::Failed);
        CHECK (status.activeProfile == 0);
        CHECK (activeIsSimple (engine));
        CHECK (live.lastFinite);
        CHECK (live.lastPeak > 1.0e-4f);   // the Simple path keeps sounding
    }

    SECTION ("negative index")
    {
        RenderEngine engine;
        prepareBinaural (engine, nonExistent.dir);
        engine.setHRTFProfile (-1);
        REQUIRE (engine.waitForHRTFProfileIdle (30000));
        CHECK (engine.getHRTFProfileStatus().problem == HRTFProfileProblem::InvalidIndex);
    }

#if ! SPATIALCORE_EMBEDS_ALL_HRTF
    SECTION ("OFF build: profile 1 with an empty folder is not found")
    {
        const TempFolder empty ("failure-empty", true);
        RenderEngine engine;
        prepareBinaural (engine, empty.dir);
        engine.setHRTFProfile (1);
        REQUIRE (engine.waitForHRTFProfileIdle (30000));

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

    REQUIRE (engine.waitForHRTFProfileIdle (60000));
    CHECK (engine.getHRTFProfileStatus().requestedProfile == last);

    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, kRate), kBlock, kRate);
    REQUIRE (live.renderUntilActive (last, 400) > 0);
    CHECK (engine.getHRTFProfileStatus().requestedProfile == last);
    CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Ready);
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
    REQUIRE (engine.waitForHRTFProfileIdle (30000));
    REQUIRE (engine.getHRTFProfileStatus().state == HRTFLoadState::Failed);

    engine.setHRTFProfile (9);
    const HRTFLoadState straightAfter = engine.getHRTFProfileStatus().state;
    CHECK ((straightAfter == HRTFLoadState::Loading || straightAfter == HRTFLoadState::Failed));
    REQUIRE (engine.waitForHRTFProfileIdle (30000));
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

    REQUIRE (engine.waitForHRTFProfileIdle (60000));

    LiveRender live (engine, makeBinauralContext (BinauralPath::HRTF, 44100.0), kBlock, 44100.0);
    REQUIRE (live.renderUntilActive (kProfile, 400) > 0);
    live.renderAudible (50);

    CHECK (engine.getHRTFProfileStatus().state == HRTFLoadState::Ready);
    CHECK (activeIRLength (engine) == reference44.getIRLength());

    // A second prepare at the same settings must not reload anything.
    engine.prepare (44100.0, 512);
    REQUIRE (engine.waitForHRTFProfileIdle (60000));
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
    REQUIRE (engine.waitForHRTFProfileIdle (30000));
    const int blocks = live.renderUntilActive (5, 10);
    CHECK (blocks > 0);
    CHECK_FALSE (engine.isRendererCrossfadeActive());
    CHECK (live.lastFinite);

    // The renderer that was active is free again, so a second request completes.
    engine.setHRTFProfile (kProbeProfile == 5 ? 0 : kProbeProfile);
    REQUIRE (engine.waitForHRTFProfileIdle (30000));
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
    REQUIRE (engine.waitForHRTFProfileIdle (30000));
    REQUIRE (live.renderUntilActive (5, 400) > 0);
    REQUIRE (engine.isRendererCrossfadeActive());   // claimed, fading in

    live.ctx = makeBinauralContext (BinauralPath::Simple, kRate);
    live.renderOne();
    CHECK_FALSE (engine.isRendererCrossfadeActive());

    const int next = (kProbeProfile == 5) ? 0 : kProbeProfile;
    engine.setHRTFProfile (next);
    REQUIRE (engine.waitForHRTFProfileIdle (30000));   // needs the abandoned renderer back
    CHECK (live.renderUntilActive (next, 10) > 0);
    CHECK (live.lastFinite);
}
