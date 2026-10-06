#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include "../Support/RealtimeGuard.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
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

                if (! externalPositions)
                {
                    const float b = (float) blockIndex;
                    const float ft = (float) t;
                    objects[(size_t) t].azimuthDeg = wrapDegrees (-165.0f + 30.0f * ft + 5.0f * b);
                    objects[(size_t) t].elevationDeg = 60.0f * std::sin (0.2f * b + 0.5f * ft);
                    objects[(size_t) t].distance = 0.5f + 0.4f * std::sin (0.3f * b + ft);
                }

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

        // When true, fill() leaves azimuth, elevation and distance alone, so the caller writes them
        // into `objects` between blocks (Plan 05-03: positions that come from a TrajectoryEngine).
        bool externalPositions = false;

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

    constexpr std::array<OutputFormat, 6> kAmbisonicsFormats {
        OutputFormat::AmbisonicsFOA, OutputFormat::AmbisonicsSOA, OutputFormat::AmbisonicsHOA,
        OutputFormat::Ambisonics4OA, OutputFormat::Ambisonics5OA, OutputFormat::Ambisonics6OA };

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

TEST_CASE ("Realtime: every Ambisonics format renders moving distance without allocating",
           "[rt][rt-alloc][rt-lock][ambi]")
{
    requireRealtimeTrapCanary();

    for (const OutputFormat format : kAmbisonicsFormats)
    {
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kSampleRate, kBlockSize);
        engine->setOutputFormat (format);

        const RenderBlockContext ctx = makeEngineContext();
        const int numOutCh = channelsFor (format);

        MovingScene scene;
        OutputBank out;
        bool finite = false;
        const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, numOutCh,
                                                   kBlockCount, kBlockSize, finite);

        CAPTURE ((int) format, numOutCh, counts.allocs, counts.frees, counts.locks);
        CHECK (counts.allocs == 0);
        CHECK (counts.frees == 0);
        CHECK (counts.locks == 0);
        CHECK (finite);
    }
}

// ----------------------------------------------------------------------------
// D-11, RTSF-01, RTSF-05: a host block longer than prepare()'s maxBlockSize is rendered completely
// and allocates nothing, on every render path. The HRTF path used to grow three engine buffers on
// the audio thread (CR-03); it now renders the block in pieces of at most the prepared size.
// ----------------------------------------------------------------------------
namespace
{
    constexpr int kOversizedPrepared = 256;
    constexpr int kOversizedBlock = 1024;
    constexpr int kOversizedBlocks = 30;

    // In a Debug tree libmysofa is compiled with -DVDEBUG, and every HRIR interpolation then
    // printf()s (libmysofa interpolate.c:119). That allocates and takes stdio locks inside
    // libmysofa on the thread that looks the HRIR up, which is not a SpatialCore allocation. So a
    // block that looks HRIRs up is allocation- and lock-checked in Release only (the Release ctest
    // is the RTSF-01 proof); the output, the counter and every other check run in both builds.
    // Blocks that look nothing up (no live object, empty) are checked everywhere.
    constexpr bool kHRIRLookupTraces =
#if JUCE_DEBUG
        true;
#else
        false;
#endif

    // The five configurations the plan names. `hrtf` requests profile 5 (embedded KEMAR) and waits
    // for it, then the consumer sets useHRTF; the Simple configuration loads nothing.
    struct OversizedConfig
    {
        const char* name;
        OutputFormat format;
        bool hrtf;
        bool useHRTF;
    };

    constexpr std::array<OversizedConfig, 5> kOversizedConfigs { {
        { "Simple binaural",     OutputFormat::Binaural,       false, false },
        { "HRTF binaural",       OutputFormat::Binaural,       true,  true  },
        { "Stereo variant",      OutputFormat::Stereo,         false, false },
        { "Discrete surround",   OutputFormat::Surround7_1_4,  false, false },
        { "Ambisonics HOA",      OutputFormat::AmbisonicsHOA,  false, false } } };

    std::unique_ptr<RenderEngine> makeOversizedEngine (const OversizedConfig& config, int prepared)
    {
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kSampleRate, prepared);
        engine->setOutputFormat (config.format);

        if (config.hrtf)
        {
            engine->setHRTFProfile (5);
            REQUIRE (engine->waitForHRTFProfileIdle (30000));
        }

        return engine;
    }

    bool anyNonzeroOut (const OutputBank& out, int numCh, int n)
    {
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
                if (out.channels[(size_t) c][(size_t) i] != 0.0f)
                    return true;

        return false;
    }
}

TEST_CASE ("Realtime: an oversized block renders on every path without allocating (D-11)",
           "[rt][rt-alloc][rt-lock][oversized]")
{
    requireRealtimeTrapCanary();

    for (const auto& config : kOversizedConfigs)
    {
        auto engine = makeOversizedEngine (config, kOversizedPrepared);

        RenderBlockContext ctx = makeEngineContext();
        ctx.useHRTF = config.useHRTF;
        const int numOutCh = channelsFor (config.format);

        MovingScene scene;
        OutputBank out;
        bool finite = false;
        const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, numOutCh,
                                                   kOversizedBlocks, kOversizedBlock, finite);

        INFO (config.name);
        CAPTURE (counts.allocs, counts.frees, counts.locks);

        if (! (config.hrtf && kHRIRLookupTraces))
        {
            CHECK (counts.allocs == 0);
            CHECK (counts.frees == 0);
            CHECK (counts.locks == 0);
        }

        CHECK (finite);
        CHECK (anyNonzeroOut (out, numOutCh, kOversizedBlock));
        CHECK (engine->getOversizedBlockCount() == kOversizedBlocks);
    }
}

namespace
{
    // One renderBlock call with the trap armed around it only; the counts are those of this call.
    RealtimeCounts armedOnce (RenderEngine& engine, const RenderSources& sources,
                              const RenderBlockContext& ctx, OutputBank& out, int numOutCh)
    {
        resetRealtimeCounts();

        {
            const ScopedRealtimeTrap trap;
            engine.renderBlock (sources, ctx, out.ptrs, numOutCh);
        }

        return realtimeCounts();
    }

    // True when sample `index` of every channel in [firstCh, lastCh] equals `value`.
    bool channelsAllEqualAt (const OutputBank& out, int firstCh, int lastCh, int index, float value)
    {
        for (int c = firstCh; c <= lastCh; ++c)
            if (out.channels[(size_t) c][(size_t) index] != value)
                return false;

        return true;
    }

    bool channelsAllEqual (const OutputBank& out, int firstCh, int lastCh, int n, float value)
    {
        for (int c = firstCh; c <= lastCh; ++c)
            for (int i = 0; i < n; ++i)
                if (out.channels[(size_t) c][(size_t) i] != value)
                    return false;

        return true;
    }
}

TEST_CASE ("Realtime: block-length edges, exactly prepared, one sample over, empty, no live object (D-11)",
           "[rt][rt-alloc][rt-lock][oversized]")
{
    requireRealtimeTrapCanary();

    SECTION ("HRTF path: prepared size, one over, empty, no live object")
    {
        const OversizedConfig& config = kOversizedConfigs[1];
        auto engine = makeOversizedEngine (config, kOversizedPrepared);

        RenderBlockContext ctx = makeEngineContext();
        ctx.useHRTF = config.useHRTF;
        const int numOutCh = channelsFor (config.format);

        MovingScene scene;
        OutputBank out;

        // Exactly the prepared size: one piece, not counted.
        {
            scene.fill (0, kOversizedPrepared);
            out.clear();
            const RealtimeCounts counts = armedOnce (*engine, scene.sources (kOversizedPrepared), ctx, out, numOutCh);
            CAPTURE (counts.allocs, counts.frees, counts.locks);
            if (! kHRIRLookupTraces)
            {
                CHECK (counts.allocs == 0);
                CHECK (counts.frees == 0);
                CHECK (counts.locks == 0);
            }
            CHECK (out.allFinite (numOutCh, kOversizedPrepared));
            CHECK (engine->getOversizedBlockCount() == 0);
        }

        // One sample over: two pieces (256 and 1), counted once, nothing allocated.
        {
            scene.fill (1, kOversizedPrepared + 1);
            out.clear();
            const RealtimeCounts counts = armedOnce (*engine, scene.sources (kOversizedPrepared + 1), ctx, out, numOutCh);
            CAPTURE (counts.allocs, counts.frees, counts.locks);
            if (! kHRIRLookupTraces)
            {
                CHECK (counts.allocs == 0);
                CHECK (counts.frees == 0);
                CHECK (counts.locks == 0);
            }
            CHECK (out.allFinite (numOutCh, kOversizedPrepared + 1));
            CHECK (engine->getOversizedBlockCount() == 1);
        }

        // Empty: nothing written, nothing allocated, nothing counted.
        {
            for (auto& ch : out.channels)
                std::fill (ch.begin(), ch.end(), 7.0f);

            scene.fill (2, kOversizedPrepared);
            const RealtimeCounts counts = armedOnce (*engine, scene.sources (0), ctx, out, numOutCh);
            CAPTURE (counts.allocs, counts.frees, counts.locks);
            CHECK (counts.allocs == 0);
            CHECK (counts.frees == 0);
            CHECK (counts.locks == 0);
            CHECK (channelsAllEqual (out, 0, numOutCh - 1, kOversizedBlock, 7.0f));
            CHECK (engine->getOversizedBlockCount() == 1);
        }

        // An oversized block with no live object: exact silence on channels 0 and 1.
        {
            for (auto& ch : out.channels)
                std::fill (ch.begin(), ch.end(), 7.0f);

            RenderSources none;
            none.numSamples = kOversizedBlock;
            const RealtimeCounts counts = armedOnce (*engine, none, ctx, out, numOutCh);
            CAPTURE (counts.allocs, counts.frees, counts.locks);
            CHECK (counts.allocs == 0);
            CHECK (counts.frees == 0);
            CHECK (counts.locks == 0);
            CHECK (channelsAllEqual (out, 0, 1, kOversizedBlock, 0.0f));
            CHECK (engine->getOversizedBlockCount() == 2);
        }
    }

    SECTION ("a zero-length block does not consume the SC-20 format-switch fade")
    {
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kSampleRate, kOversizedPrepared);
        engine->setOutputFormat (OutputFormat::Binaural);

        RenderBlockContext ctx = makeEngineContext();
        ctx.engineFadesFormatSwitch = true;

        MovingScene scene;
        OutputBank out;

        // A first real block, so the engine has played something (the first block never fades).
        scene.fill (0, kOversizedPrepared);
        engine->renderBlock (scene.sources (kOversizedPrepared), ctx, out.ptrs, 2);

        engine->setOutputFormat (OutputFormat::Surround7_1_4);

        // The empty block between the switch and the next real block.
        scene.fill (1, kOversizedPrepared);
        out.clear();
        const RealtimeCounts counts = armedOnce (*engine, scene.sources (0), ctx, out, 12);
        CAPTURE (counts.allocs, counts.frees, counts.locks);
        CHECK (counts.allocs == 0);
        CHECK (counts.locks == 0);
        CHECK (engine->getOversizedBlockCount() == 0);

        // The next real block is still the fade-out block: it renders the layout the engine held
        // (Binaural) and ends at exactly 0 on channels 0 and 1.
        scene.fill (2, kOversizedPrepared);
        out.clear();
        engine->renderBlock (scene.sources (kOversizedPrepared), ctx, out.ptrs, 12);
        CHECK (engine->getBlockLayout().format == OutputFormat::Binaural);
        CHECK (out.channels[0][kOversizedPrepared - 1] == 0.0f);
        CHECK (out.channels[1][kOversizedPrepared - 1] == 0.0f);

        bool audible = false;
        for (int i = 0; i < kOversizedPrepared - 1; ++i)
            audible = audible || out.channels[0][(size_t) i] != 0.0f || out.channels[1][(size_t) i] != 0.0f;
        CHECK (audible);

        // And the block after it is the fade-in block of the new format.
        scene.fill (3, kOversizedPrepared);
        out.clear();
        engine->renderBlock (scene.sources (kOversizedPrepared), ctx, out.ptrs, 12);
        CHECK (engine->getBlockLayout().format == OutputFormat::Surround7_1_4);
        CHECK (out.channels[0][0] == 0.0f);
        CHECK (out.channels[1][0] == 0.0f);
    }
}

// ----------------------------------------------------------------------------
// D-10: the NFC fix must store bit-for-bit the coefficients the old temporary stored.
// The formula below is a test-local copy of the update in RenderEngine::renderAmbisonicsOutput
// (same clamps, same float operations, c = 343, reference radius 1.5 m).
// ----------------------------------------------------------------------------
TEST_CASE ("Ambisonics NFC: the in-place array assignment stores the old coefficients bit for bit",
           "[ambi][nfc][d10]")
{
    constexpr float c = 343.0f;
    constexpr float referenceRadius = 1.5f;
    constexpr float pi = juce::MathConstants<float>::pi;
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const float distances[] = { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 5.0f, 10.0f };

    int combinations = 0;

    for (const double sr : rates)
    {
        for (int n = 1; n <= 6; ++n)
        {
            for (const float distance : distances)
            {
                const float r = std::max (0.05f, distance);
                float fPole = static_cast<float> (n) * c / (2.0f * pi * r);
                float fZero = static_cast<float> (n) * c / (2.0f * pi * referenceRadius);
                const float maxFreq = static_cast<float> (sr) * 0.25f;
                fPole = std::min (fPole, maxFreq);
                fZero = std::min (fZero, maxFreq);
                const float wPole = std::tan (pi * fPole / static_cast<float> (sr));
                const float wZero = std::tan (pi * fZero / static_cast<float> (sr));
                const float b0 = (1.0f + wZero);
                const float b1 = (wZero - 1.0f);
                const float a0 = (1.0f + wPole);
                const float a1 = (wPole - 1.0f);

                const juce::dsp::IIR::Coefficients<float> oldWay (b0 / a0, b1 / a0, 1.0f, a1 / a0);

                // prepare() installs this placeholder; the render path then assigns the array into it.
                juce::dsp::IIR::Coefficients<float> newWay (1.0f, 0.0f, 1.0f, 0.0f);
                newWay = std::array<float, 4> { b0 / a0, b1 / a0, 1.0f, a1 / a0 };

                CAPTURE (sr, n, distance);
                REQUIRE (oldWay.coefficients.size() == newWay.coefficients.size());
                REQUIRE (std::memcmp (oldWay.getRawCoefficients(), newWay.getRawCoefficients(),
                                      sizeof (float) * (size_t) oldWay.coefficients.size()) == 0);
                ++combinations;
            }
        }
    }

    CHECK (combinations == 3 * 6 * 9);
}

// ----------------------------------------------------------------------------
// Hidden capture (not run by default, not registered by ctest): writes the six Ambisonics
// formats' raw output to the file named by SC_CAPTURE_FILE, so the before and after of the NFC
// fix can be compared with cmp (D-10). Same deterministic run as the [ambi] case, no trap.
//   SC_CAPTURE_FILE=/tmp/x.bin ./build/tests/SpatialCoreTests "[ambi-nfc-capture]"
// ----------------------------------------------------------------------------
TEST_CASE ("Ambisonics NFC: capture the six formats' raw output to SC_CAPTURE_FILE",
           "[.][ambi-nfc-capture]")
{
    const char* path = std::getenv ("SC_CAPTURE_FILE");
    REQUIRE (path != nullptr);

    const juce::File file (juce::String::fromUTF8 (path));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());

    for (const OutputFormat format : kAmbisonicsFormats)
    {
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kSampleRate, kBlockSize);
        engine->setOutputFormat (format);

        const RenderBlockContext ctx = makeEngineContext();
        const int numOutCh = channelsFor (format);

        MovingScene scene;
        OutputBank out;

        for (int block = 0; block < kBlockCount; ++block)
        {
            scene.fill (block, kBlockSize);
            out.clear();
            const RenderSources sources = scene.sources (kBlockSize);
            engine->renderBlock (sources, ctx, out.ptrs, numOutCh);

            for (int ch = 0; ch < numOutCh; ++ch)
                REQUIRE (stream.write (out.channels[(size_t) ch].data(),
                                       sizeof (float) * (size_t) kBlockSize));
        }
    }

    stream.flush();
}

// ============================================================================
// Plan 05-03 (RTSF-05, D-02): the whole audio path. Mid-play switches, then the matrix.
// ============================================================================
namespace
{
    /** SPATIALCORE_TEST_TIMEOUT_SCALE: an integer of at least 1 (default 1; anything else counts as
        1). Same rules as timeoutScale() in ProfileSwitchTests.cpp (a local copy: that one is private
        to its translation unit). It stretches the idle waits for a profile load. */
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

    /** One engine, one scene, one output bank, rendered block by block with the trap armed around
        each renderBlock call only. Counts accumulate over every block until reset (blocks made on
        the test thread while disarmed, such as setHRTFProfile, are not counted). */
    struct ArmedRun
    {
        ArmedRun (RenderEngine& e, MovingScene& s, OutputBank& o, int blockSizeIn)
            : engine (e), scene (s), out (o), blockSize (blockSizeIn)
        {
            resetRealtimeCounts();
        }

        // One block. The inputs are filled and the output cleared while disarmed.
        void block (const RenderBlockContext& ctx, int numOutCh)
        {
            scene.fill (blockIndex++, blockSize);
            out.clear();
            const RenderSources sources = scene.sources (blockSize);

            {
                const ScopedRealtimeTrap trap;
                engine.renderBlock (sources, ctx, out.ptrs, numOutCh);
            }

            if (! out.allFinite (numOutCh, blockSize))
                allFinite = false;
        }

        void blocks (const RenderBlockContext& ctx, int numOutCh, int count)
        {
            for (int i = 0; i < count; ++i)
                block (ctx, numOutCh);
        }

        // The accumulated counts, with the first offending stack printed on a non-zero allocation count.
        RealtimeCounts counts() const
        {
            const RealtimeCounts c = realtimeCounts();

            if (c.allocs != 0)
                printFirstOffender();

            return c;
        }

        RenderEngine& engine;
        MovingScene& scene;
        OutputBank& out;
        int blockSize;
        int blockIndex = 0;
        bool allFinite = true;
    };

    // Debug trees build libmysofa with -DVDEBUG, which printf()s inside every HRIR lookup (the note on
    // kHRIRLookupTraces above). A case whose blocks look HRIRs up is allocation- and lock-checked in
    // Release only; its output, profile and ordering checks run in both builds.
    constexpr bool kHRIRLookupChecked = ! kHRIRLookupTraces;

    void checkNothingAllocatedOrLocked (const RealtimeCounts& counts, bool lookupsHappened)
    {
        CAPTURE (counts.allocs, counts.frees, counts.locks);

        if (lookupsHappened && ! kHRIRLookupChecked)
            return;

        CHECK (counts.allocs == 0);
        CHECK (counts.frees == 0);
        CHECK (counts.locks == 0);
    }
}

// ----------------------------------------------------------------------------
// Tracer: one engine takes every kind of mid-play switch with all three engine flags set. This is
// also the first test that sets engineComputesGains, engineDerivesDispatch and engineSelectsHRTF on
// one binaural block (the STATE milestone-audit item "no test sets all three engine flags on one
// binaural block").
// ----------------------------------------------------------------------------
TEST_CASE ("Realtime: mid-play profile and format switches allocate and lock nothing on the render thread",
           "[rt][rt-alloc][rt-lock][switch][tracer]")
{
    requireRealtimeTrapCanary();

    const int scale = timeoutScale();
    constexpr int kOutCh = 49;   // Ambisonics 6OA, the largest format in the sequence

    for (const bool fade : { true, false })
    {
        DYNAMIC_SECTION ((fade ? "with the switch fade" : "without the switch fade"))
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (kSampleRate, kBlockSize);
            engine->setOutputFormat (OutputFormat::Binaural);

            RenderBlockContext ctx = makeEngineContext();   // sampleRate, engineComputesGains, engineDerivesDispatch
            ctx.engineSelectsHRTF = true;
            ctx.engineFadesFormatSwitch = fade;

            MovingScene scene;
            OutputBank out;
            ArmedRun run (*engine, scene, out, kBlockSize);

            auto switchProfile = [&] (int profile)
            {
                engine->setHRTFProfile (profile);                                  // disarmed
                REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));          // disarmed
                run.blocks (ctx, kOutCh, 40);

                const HRTFProfileStatus status = engine->getHRTFProfileStatus();   // disarmed
                CAPTURE (profile, status.requestedProfile, status.activeProfile);
                CHECK (status.activeProfile == profile);
                CHECK (run.allFinite);
            };

            auto switchFormat = [&] (OutputFormat format)
            {
                engine->setOutputFormat (format);                                  // disarmed
                run.blocks (ctx, kOutCh, 40);
                CAPTURE (static_cast<int> (format));
                CHECK (run.allFinite);
            };

            run.blocks (ctx, kOutCh, 20);   // Simple binaural from the first block
            CHECK (run.allFinite);

            switchProfile (1);   // the first claim starts the Simple -> SADIE blend (4096 samples, 16 blocks)
            switchProfile (0);   // SADIE -> Simple
            switchProfile (3);
            switchProfile (5);

            switchFormat (OutputFormat::Surround7_1_4);
            switchFormat (OutputFormat::Ambisonics6OA);
            switchFormat (OutputFormat::Stereo);
            switchFormat (OutputFormat::Binaural);

            checkNothingAllocatedOrLocked (run.counts(), true);
            CHECK (run.allFinite);
        }
    }
}

// ----------------------------------------------------------------------------
// The matrix (D-02): every format x every flag set, every profile x both routes, block sizes x
// sample rates, every algorithm index. Each cold-start dimension is one TEST_CASE per value, so
// ctest runs it in a process of its own and process-global first use is not hidden (RESEARCH
// Pitfall 3).
// ----------------------------------------------------------------------------
namespace
{
    constexpr unsigned kComputes = 1;   // engineComputesGains
    constexpr unsigned kDerives  = 2;   // engineDerivesDispatch
    constexpr unsigned kSelects  = 4;   // engineSelectsHRTF

    // The block context a consumer would hand in for `fmt` under the flag bits. A flag that is off
    // means the consumer supplies that part itself, so the part is filled here the way a consumer
    // would: the dispatch fields mirror RenderEngine::deriveDispatchFromLayout, the gains are
    // plausible constants (the trap cares about allocation, not about the sound).
    RenderBlockContext contextFor (OutputFormat fmt, unsigned flags, double sampleRate)
    {
        RenderBlockContext ctx;
        ctx.sampleRate = sampleRate;
        ctx.engineComputesGains = (flags & kComputes) != 0;
        ctx.engineDerivesDispatch = (flags & kDerives) != 0;
        ctx.engineSelectsHRTF = (flags & kSelects) != 0;

        const OutputFormatInfo& info = OutputFormatRegistry::getInfo (fmt);

        if (! ctx.engineDerivesDispatch)
        {
            ctx.activeFormat = fmt;
            ctx.ambiOrder = info.ambiOrder;
            ctx.isStereoVariant = info.isStereoVariant;
            ctx.isBinaural = (fmt == OutputFormat::Binaural);
            ctx.isAmbiOutput = info.isAmbisonicsOutput;
        }

        if (! ctx.engineComputesGains)
        {
            const int channels = std::min (info.requiredChannels, MAX_SPEAKERS);

            for (int t = 0; t < MAX_SOURCES; ++t)
            {
                ctx.objGains[t] = BinauralGains { 0.5f, 0.5f, 0.0f, 0.0f };
                ctx.objGainL[t] = 0.5f;
                ctx.objGainR[t] = 0.5f;

                for (int ch = 0; ch < channels; ++ch)
                    ctx.objChannelGains[t][ch] = 0.25f;
            }

            ctx.stereoMode = 0;
        }

        return ctx;
    }

    // One format, all eight flag sets, each a fresh heap engine armed from its first block.
    void runFormatMatrix (OutputFormat fmt)
    {
        requireRealtimeTrapCanary();

        const int numOutCh = channelsFor (fmt);

        for (unsigned flags = 0; flags < 8; ++flags)
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (kSampleRate, kBlockSize);
            engine->setOutputFormat (fmt);

            const RenderBlockContext ctx = contextFor (fmt, flags, kSampleRate);

            MovingScene scene;
            OutputBank out;
            bool finite = false;
            const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, numOutCh,
                                                       kBlockCount, kBlockSize, finite);

            CAPTURE (static_cast<int> (fmt), flags, numOutCh);
            checkNothingAllocatedOrLocked (counts, false);
            CHECK (finite);
        }
    }

    // One profile on both routes. "engine-selected": all three flags. "consumer-selected":
    // OpenSpatialDelay's route, engineSelectsHRTF off and useHRTF on for the SOFA profiles.
    void runProfileMatrix (int profile)
    {
        requireRealtimeTrapCanary();

        const int scale = timeoutScale();
        const bool lookups = profile != 0;   // a SOFA profile looks an HRIR up for every object, every block

        struct Route { const char* name; unsigned flags; bool useHRTF; };
        const Route routes[] = {
            { "engine-selected",   kComputes | kDerives | kSelects, false },
            { "consumer-selected", kComputes | kDerives,            profile != 0 } };

        for (const Route& route : routes)
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (kSampleRate, kBlockSize);
            engine->setOutputFormat (OutputFormat::Binaural);
            engine->setHRTFProfile (profile);                                  // disarmed
            REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));          // disarmed

            RenderBlockContext ctx = contextFor (OutputFormat::Binaural, route.flags, kSampleRate);
            ctx.useHRTF = route.useHRTF;

            MovingScene scene;
            OutputBank out;
            bool finite = false;
            const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, 2,
                                                       kBlockCount, kBlockSize, finite);

            INFO (route.name);
            CAPTURE (profile);
            checkNothingAllocatedOrLocked (counts, lookups);
            CHECK (finite);
            CHECK (engine->getHRTFProfileStatus().activeProfile == profile);
        }

        // D-10: libmysofa's per-block HRIR lookup at the other two rates a host runs (the 48 kHz
        // run above is the cold-start one). Engine-selected route, same 120 moving blocks.
        for (const double rate : { 44100.0, 96000.0 })
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (rate, kBlockSize);
            engine->setOutputFormat (OutputFormat::Binaural);
            engine->setHRTFProfile (profile);
            REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));

            const RenderBlockContext ctx = contextFor (OutputFormat::Binaural, kComputes | kDerives | kSelects, rate);

            MovingScene scene (rate);
            OutputBank out;
            bool finite = false;
            const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, 2,
                                                       kBlockCount, kBlockSize, finite);

            CAPTURE (profile, rate);
            checkNothingAllocatedOrLocked (counts, lookups);
            CHECK (finite);
            CHECK (engine->getHRTFProfileStatus().activeProfile == profile);
        }
    }
}

#define SC_RT_FORMAT_CASE(Name) \
    TEST_CASE ("Realtime: " #Name " renders every flag combination without allocating or locking", \
               "[rt][rt-alloc][rt-lock][matrix]") \
    { \
        runFormatMatrix (OutputFormat::Name); \
    }

// A new OutputFormat fails the build here until it gets its own case below.
static_assert (NUM_OUTPUT_FORMATS == 23, "a new OutputFormat needs an SC_RT_FORMAT_CASE line");

SC_RT_FORMAT_CASE (Binaural)
SC_RT_FORMAT_CASE (Stereo)
SC_RT_FORMAT_CASE (Quad)
SC_RT_FORMAT_CASE (Surround5_0)
SC_RT_FORMAT_CASE (Surround5_1)
SC_RT_FORMAT_CASE (Surround7_0)
SC_RT_FORMAT_CASE (Surround7_1)
SC_RT_FORMAT_CASE (Surround9_1)
SC_RT_FORMAT_CASE (Octaphonic)
SC_RT_FORMAT_CASE (Surround5_1_2)
SC_RT_FORMAT_CASE (Surround5_1_4)
SC_RT_FORMAT_CASE (Surround7_1_2)
SC_RT_FORMAT_CASE (Surround7_1_4)
SC_RT_FORMAT_CASE (Surround7_1_6)
SC_RT_FORMAT_CASE (Surround9_1_4)
SC_RT_FORMAT_CASE (Surround9_1_6)
SC_RT_FORMAT_CASE (SurroundSML13_1)
SC_RT_FORMAT_CASE (AmbisonicsFOA)
SC_RT_FORMAT_CASE (AmbisonicsSOA)
SC_RT_FORMAT_CASE (AmbisonicsHOA)
SC_RT_FORMAT_CASE (Ambisonics4OA)
SC_RT_FORMAT_CASE (Ambisonics5OA)
SC_RT_FORMAT_CASE (Ambisonics6OA)

#define SC_RT_PROFILE_CASE(N) \
    TEST_CASE ("Realtime: HRTF profile " #N " renders on both routes without allocating or locking", \
               "[rt][rt-alloc][rt-lock][matrix][profiles]") \
    { \
        runProfileMatrix (N); \
    }

SC_RT_PROFILE_CASE (0)
SC_RT_PROFILE_CASE (1)
SC_RT_PROFILE_CASE (2)
SC_RT_PROFILE_CASE (3)
SC_RT_PROFILE_CASE (4)
SC_RT_PROFILE_CASE (5)

TEST_CASE ("Realtime: every block size and sample rate renders without allocating",
           "[rt][rt-alloc][rt-lock][matrix][sizes]")
{
    requireRealtimeTrapCanary();

    struct Config { const char* name; OutputFormat format; unsigned flags; bool hrtf; };
    const Config configs[] = {
        { "Binaural profile 5, all flags", OutputFormat::Binaural,      kComputes | kDerives | kSelects, true  },
        { "7.1.4",                         OutputFormat::Surround7_1_4, kComputes | kDerives,            false },
        { "Ambisonics HOA",                OutputFormat::AmbisonicsHOA, kComputes | kDerives,            false } };

    const int scale = timeoutScale();

    for (const int blockSize : { 32, 64, 256, 512, 1024 })
    {
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            for (const Config& config : configs)
            {
                auto engine = std::make_unique<RenderEngine>();
                engine->prepare (rate, blockSize);
                engine->setOutputFormat (config.format);

                if (config.hrtf)
                {
                    engine->setHRTFProfile (5);
                    REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));
                }

                const RenderBlockContext ctx = contextFor (config.format, config.flags, rate);
                const int numOutCh = channelsFor (config.format);

                MovingScene scene (rate);
                OutputBank out;
                bool finite = false;
                const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, numOutCh,
                                                           60, blockSize, finite);

                INFO (config.name);
                CAPTURE (blockSize, rate);
                checkNothingAllocatedOrLocked (counts, config.hrtf);
                CHECK (finite);
            }
        }
    }
}

TEST_CASE ("Realtime: all twelve algorithm indices render every non-binaural format without allocating",
           "[rt][rt-alloc][rt-lock][matrix][algorithms]")
{
    requireRealtimeTrapCanary();

    for (int formatIndex = 0; formatIndex < NUM_OUTPUT_FORMATS; ++formatIndex)
    {
        const auto format = static_cast<OutputFormat> (formatIndex);

        if (format == OutputFormat::Binaural)
            continue;

        const int numOutCh = channelsFor (format);
        const RenderBlockContext ctx = contextFor (format, kComputes | kDerives, kSampleRate);

        for (int index = 0; index < kNumAlgorithmIndices; ++index)
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (kSampleRate, kBlockSize);
            engine->setOutputFormat (format);
            engine->setAlgorithmIndex (index);   // disarmed

            MovingScene scene;
            OutputBank out;
            bool finite = false;
            const RealtimeCounts counts = armedRender (*engine, scene, ctx, out, numOutCh,
                                                       40, kBlockSize, finite);

            CAPTURE (formatIndex, index);
            checkNothingAllocatedOrLocked (counts, false);
            CHECK (finite);
        }
    }
}

// ----------------------------------------------------------------------------
// D-10: AmbisonicsAlgorithm::computeGains used a function-local `static const float maxrE[4]` built
// with std::cos. Its first call in a process went through __cxa_guard_acquire, which takes a mutex:
// a lock on the render thread the first time the Ambisonics algorithm (index 0) renders on a speaker
// format. The weights are now constants. The sound must be bit-for-bit what it was.
//
// referenceAmbisonicsGains is a test-local copy of the old body (same expressions, same operation
// order, the old static). The 25 x 13 direction grid below is compared against the engine's
// algorithm with memcmp, with a non-trivial decode matrix; a hidden capture writes the same grid
// to SC_CAPTURE_FILE so a build from before the fix and one from after can be compared with cmp.
// ----------------------------------------------------------------------------
namespace
{
    void referenceAmbisonicsGains (const SourcePosition& source, const float (*decode)[MAX_SPEAKERS],
                                   float* outputGains, int numSpeakers)
    {
        constexpr int HOA_CH = 16;

        static const float maxrE[4] = {
            1.0f,
            std::cos (juce::MathConstants<float>::pi / 8.0f),
            std::cos (2.0f * juce::MathConstants<float>::pi / 8.0f),
            std::cos (3.0f * juce::MathConstants<float>::pi / 8.0f),
        };
        auto acnToOrder = [] (int acn) -> int {
            if (acn < 1) return 0; if (acn < 4) return 1;
            if (acn < 9) return 2; return 3;
        };

        if (! std::isfinite (source.azimuthRad) || ! std::isfinite (source.elevationRad))
        {
            for (int s = 0; s < numSpeakers; ++s)
                outputGains[s] = 0.0f;
            return;
        }

        float coeffs[HOA_CH];
        for (int c = 0; c < HOA_CH; ++c)
            coeffs[c] = evalSH (c, source.azimuthRad, source.elevationRad) * maxrE[acnToOrder (c)];

        float totalPower = 0.0f;
        for (int s = 0; s < numSpeakers; ++s)
        {
            float gain = 0.0f;
            for (int c = 0; c < HOA_CH; ++c)
                gain += decode[s][c] * coeffs[c];

            outputGains[s] = std::max (0.0f, gain);
            totalPower += outputGains[s] * outputGains[s];
        }

        if (totalPower > 1e-12f)
        {
            float scale = 1.0f / std::sqrt (totalPower);
            for (int s = 0; s < numSpeakers; ++s)
                outputGains[s] *= scale;
        }
    }

    // A fixed, non-trivial 12 x 16 decode matrix (a 7.1.4 layout has 12 speakers), no randomness.
    void fillDecodeMatrix (float (&m)[MAX_SPEAKERS][MAX_SPEAKERS])
    {
        for (int s = 0; s < MAX_SPEAKERS; ++s)
            for (int c = 0; c < MAX_SPEAKERS; ++c)
                m[s][c] = 0.4f * std::sin (0.7f * (float) s + 1.3f * (float) c + 0.2f)
                          + 0.1f * (float) ((s * 7 + c * 3) % 5) - 0.15f;
    }

    constexpr int kAmbiGridAz = 25;   // -180 .. 180 in steps of 15
    constexpr int kAmbiGridEl = 13;   // -90 .. 90 in steps of 15
    constexpr int kAmbiSpeakers = 12;

    SourcePosition gridPosition (int a, int e)
    {
        return { juce::degreesToRadians (-180.0f + 15.0f * (float) a),
                 juce::degreesToRadians (-90.0f + 15.0f * (float) e), 0.5f };
    }
}

TEST_CASE ("Ambisonics algorithm: constant max-rE weights give the old gains bit for bit",
           "[ambi][maxre][d10]")
{
    float decode[MAX_SPEAKERS][MAX_SPEAKERS];
    fillDecodeMatrix (decode);

    SpeakerLayout layout {};
    const std::vector<VBAPTriplet> triplets;
    const LayoutContext ctx { layout, triplets, decode, kAmbiSpeakers };
    const AmbisonicsAlgorithm algorithm;

    int compared = 0;

    for (int a = 0; a < kAmbiGridAz; ++a)
    {
        for (int e = 0; e < kAmbiGridEl; ++e)
        {
            const SourcePosition source = gridPosition (a, e);
            float expected[kAmbiSpeakers] = {};
            float actual[kAmbiSpeakers] = {};
            referenceAmbisonicsGains (source, decode, expected, kAmbiSpeakers);
            algorithm.computeGains (source, ctx, actual, kAmbiSpeakers);

            CAPTURE (a, e);
            REQUIRE (std::memcmp (expected, actual, sizeof (expected)) == 0);
            ++compared;
        }
    }

    CHECK (compared == kAmbiGridAz * kAmbiGridEl);
}

// Hidden capture (not run by default, not registered by ctest):
//   SC_CAPTURE_FILE=/tmp/x.bin ./build/tests/SpatialCoreTests "[ambi-maxre-capture]"
TEST_CASE ("Ambisonics algorithm: capture the grid of gains to SC_CAPTURE_FILE",
           "[.][ambi-maxre-capture]")
{
    const char* path = std::getenv ("SC_CAPTURE_FILE");
    REQUIRE (path != nullptr);

    const juce::File file (juce::String::fromUTF8 (path));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());

    float decode[MAX_SPEAKERS][MAX_SPEAKERS];
    fillDecodeMatrix (decode);

    SpeakerLayout layout {};
    const std::vector<VBAPTriplet> triplets;
    const LayoutContext ctx { layout, triplets, decode, kAmbiSpeakers };
    const AmbisonicsAlgorithm algorithm;

    for (int a = 0; a < kAmbiGridAz; ++a)
    {
        for (int e = 0; e < kAmbiGridEl; ++e)
        {
            float gains[kAmbiSpeakers] = {};
            algorithm.computeGains (gridPosition (a, e), ctx, gains, kAmbiSpeakers);
            REQUIRE (stream.write (gains, sizeof (gains)));
        }
    }

    stream.flush();
}

// ----------------------------------------------------------------------------
// Trajectories (D-02): positions that come from a TrajectoryEngine animating at the demo slider's
// maximum speed, copied into the scene between blocks the way examples/demo/DemoComponent.cpp does
// (wrapAzimuth (getFinalAz), getFinalEl, getFinalDist). Pass A gives object t shape 1 + t (the
// twelve shapes 1 to 12), pass B gives every object shape 13, so all 13 shapes run; odd objects run
// in reverse. The tick is made on the test thread, disarmed, as the demo makes it on its timer.
// ----------------------------------------------------------------------------
TEST_CASE ("Realtime: trajectory-driven positions render without allocating",
           "[rt][rt-alloc][rt-lock][trajectory]")
{
    requireRealtimeTrapCanary();

    struct Config { const char* name; OutputFormat format; unsigned flags; bool hrtf; };
    const Config configs[] = {
        { "Binaural profile 5, all flags", OutputFormat::Binaural,       kComputes | kDerives | kSelects, true  },
        { "7.1.4",                         OutputFormat::Surround7_1_4,  kComputes | kDerives,            false },
        { "Ambisonics 6OA",                OutputFormat::Ambisonics6OA,  kComputes | kDerives,            false } };

    constexpr float kSpeed = 4.0f;   // examples/demo/DemoComponent.cpp: the speed slider's maximum
    constexpr float dt = (float) kBlockSize / (float) kSampleRate;
    const int scale = timeoutScale();

    for (const int pass : { 0, 1 })
    {
        for (const Config& config : configs)
        {
            auto engine = std::make_unique<RenderEngine>();
            engine->prepare (kSampleRate, kBlockSize);
            engine->setOutputFormat (config.format);

            if (config.hrtf)
            {
                engine->setHRTFProfile (5);
                REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));
            }

            const RenderBlockContext ctx = contextFor (config.format, config.flags, kSampleRate);
            const int numOutCh = channelsFor (config.format);

            auto trajectory = std::make_unique<TrajectoryEngine>();
            MovingScene scene;
            scene.externalPositions = true;
            OutputBank out;
            ArmedRun run (*engine, scene, out, kBlockSize);
            bool positionsFinite = true;

            for (int block = 0; block < kBlockCount; ++block)
            {
                for (int t = 0; t < MAX_SOURCES; ++t)
                {
                    TrajectoryEngine::ObjectInput in;
                    in.shape = pass == 0 ? 1 + t : 13;
                    in.speed = kSpeed;
                    in.reverse = (t % 2) == 1;
                    in.originAz = wrapDegrees (-165.0f + 30.0f * (float) t);
                    in.originEl = 30.0f * std::sin ((float) t);
                    in.originDist = 0.4f + 0.05f * (float) (t % 6);
                    trajectory->tick (t, in, dt);   // disarmed

                    ObjectState& object = scene.objects[(size_t) t];
                    object.azimuthDeg = wrapAzimuth (trajectory->getFinalAz (t));
                    object.elevationDeg = trajectory->getFinalEl (t);
                    object.distance = trajectory->getFinalDist (t);

                    positionsFinite = positionsFinite && std::isfinite (object.azimuthDeg)
                                      && std::isfinite (object.elevationDeg) && std::isfinite (object.distance);
                }

                run.block (ctx, numOutCh);
            }

            INFO (config.name);
            CAPTURE (pass);
            checkNothingAllocatedOrLocked (run.counts(), config.hrtf);
            CHECK (positionsFinite);
            CHECK (run.allFinite);
        }
    }
}

// ----------------------------------------------------------------------------
// The audit item "Binaural -> non-binaural switch during an HRTF renderer crossfade is untested":
// a Binaural -> 7.1.4 switch lands while the SADIE -> KEMAR renderer crossfade is running, with the
// SC-20 switch fade and all three flags. The fade-out block ends at exactly 0 on the binaural
// channels, the fade-in block starts at exactly 0 on every 7.1.4 channel, nothing allocates, and no
// renderer is left stranded: a later profile request still becomes active.
// ----------------------------------------------------------------------------
TEST_CASE ("Realtime: a Binaural to 7.1.4 switch during an HRTF renderer crossfade fades cleanly and strands nothing",
           "[rt][rt-alloc][rt-lock][switch][audit]")
{
    requireRealtimeTrapCanary();

    const int scale = timeoutScale();
    const int numOutCh = channelsFor (OutputFormat::Surround7_1_4);

    auto engine = std::make_unique<RenderEngine>();
    engine->prepare (kSampleRate, kBlockSize);
    engine->setOutputFormat (OutputFormat::Binaural);

    RenderBlockContext ctx = makeEngineContext();
    ctx.engineSelectsHRTF = true;
    ctx.engineFadesFormatSwitch = true;

    MovingScene scene;
    OutputBank out;
    ArmedRun run (*engine, scene, out, kBlockSize);

    run.blocks (ctx, numOutCh, 20);

    engine->setHRTFProfile (1);
    REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));
    run.blocks (ctx, numOutCh, 40);   // SADIE active and settled
    REQUIRE (engine->getHRTFProfileStatus().activeProfile == 1);

    engine->setHRTFProfile (5);
    REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));
    run.blocks (ctx, numOutCh, 2);    // the first block claims KEMAR and starts the 4096-sample crossfade
    REQUIRE (engine->isRendererCrossfadeActive());

    engine->setOutputFormat (OutputFormat::Surround7_1_4);   // the switch lands mid-crossfade

    run.block (ctx, numOutCh);   // the fade-out block: still the binaural layout
    CHECK (engine->getBlockLayout().format == OutputFormat::Binaural);
    CHECK (out.channels[0][(size_t) kBlockSize - 1] == 0.0f);
    CHECK (out.channels[1][(size_t) kBlockSize - 1] == 0.0f);

    run.block (ctx, numOutCh);   // the fade-in block: the new layout, starting from exact zero
    CHECK (engine->getBlockLayout().format == OutputFormat::Surround7_1_4);
    CHECK (channelsAllEqualAt (out, 0, numOutCh - 1, 0, 0.0f));

    run.blocks (ctx, numOutCh, 40);

    engine->setOutputFormat (OutputFormat::Binaural);
    run.blocks (ctx, numOutCh, 40);

    engine->setHRTFProfile (3);
    REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));

    bool becameThree = false;

    for (int i = 0; i < 200 && ! becameThree; ++i)
    {
        run.block (ctx, numOutCh);
        becameThree = engine->getHRTFProfileStatus().activeProfile == 3;   // disarmed
    }

    CHECK (becameThree);
    checkNothingAllocatedOrLocked (run.counts(), true);
    CHECK (run.allFinite);
}

// ----------------------------------------------------------------------------
// Ordering (RTSF-05 probe): several requests between two blocks. The last one wins and an
// intermediate one never becomes the rendered state.
// ----------------------------------------------------------------------------
TEST_CASE ("Realtime: several requests between two blocks: the last one wins with no allocation",
           "[rt][rt-alloc][rt-lock][switch][ordering]")
{
    requireRealtimeTrapCanary();

    const int scale = timeoutScale();

    SECTION ("three output-format requests give one render, of the last")
    {
        constexpr int kQuadChannels = 4;

        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kSampleRate, kBlockSize);
        engine->setOutputFormat (OutputFormat::Binaural);

        const RenderBlockContext ctx = makeEngineContext();   // engineDerivesDispatch is set

        MovingScene scene;
        OutputBank out;
        ArmedRun run (*engine, scene, out, kBlockSize);

        run.blocks (ctx, kQuadChannels, 10);
        CHECK (engine->getBlockLayout().format == OutputFormat::Binaural);

        engine->setOutputFormat (OutputFormat::Surround7_1_4);
        engine->setOutputFormat (OutputFormat::Ambisonics6OA);
        engine->setOutputFormat (OutputFormat::Quad);

        run.block (ctx, kQuadChannels);
        CHECK (engine->getBlockLayout().format == OutputFormat::Quad);

        checkNothingAllocatedOrLocked (run.counts(), false);
        CHECK (run.allFinite);
    }

    SECTION ("two profile requests make the second active and never the first")
    {
        auto engine = std::make_unique<RenderEngine>();
        engine->prepare (kSampleRate, kBlockSize);
        engine->setOutputFormat (OutputFormat::Binaural);

        RenderBlockContext ctx = makeEngineContext();
        ctx.engineSelectsHRTF = true;

        MovingScene scene;
        OutputBank out;
        ArmedRun run (*engine, scene, out, kBlockSize);

        engine->setHRTFProfile (3);
        engine->setHRTFProfile (5);
        REQUIRE (engine->waitForHRTFProfileIdle (60000 * scale));

        bool becameFive = false;
        bool sawThree = false;

        for (int i = 0; i < 200 && ! becameFive; ++i)
        {
            run.block (ctx, 2);
            const int active = engine->getHRTFProfileStatus().activeProfile;   // disarmed
            sawThree = sawThree || active == 3;
            becameFive = active == 5;
        }

        CHECK (becameFive);
        CHECK_FALSE (sawThree);
        checkNothingAllocatedOrLocked (run.counts(), true);
        CHECK (run.allFinite);
    }
}

// ----------------------------------------------------------------------------
// D-09: OpenSpatialDelay's route stays working. It loads into the renderer the audio thread is not
// using, on its own thread, and swaps; the render thread then runs the renderer crossfade.
// ----------------------------------------------------------------------------
TEST_CASE ("Realtime: the legacy load-and-swap route renders its crossfade without allocating",
           "[rt][rt-alloc][rt-lock][legacy]")
{
    requireRealtimeTrapCanary();

    auto engine = std::make_unique<RenderEngine>();
    engine->prepare (kSampleRate, kBlockSize);
    engine->setOutputFormat (OutputFormat::Binaural);

    // The consumer supplies the dispatch itself and asks for the HRTF path; the engine computes gains.
    RenderBlockContext ctx = contextFor (OutputFormat::Binaural, kComputes, kSampleRate);
    ctx.useHRTF = true;

    MovingScene scene;
    OutputBank out;
    ArmedRun run (*engine, scene, out, kBlockSize);

    run.blocks (ctx, 2, 20);

    // Disarmed, on this (the message) thread: the consumer's own load and swap.
    BinauralRenderer& renderer = engine->getBinauralRenderer (engine->getPrepareRendererIndex());
    REQUIRE (renderer.hrtfDatabase.loadFromFile (juce::File (SPATIALCORE_HRTF_DIR "/mit_kemar_large_pinna.sofa"),
                                                 (float) kSampleRate));
    renderer.setProfile (5);
    engine->swapActiveRenderer();

    run.block (ctx, 2);        // the first block claims the swap and starts the renderer crossfade
    CHECK (engine->isRendererCrossfadeActive());
    run.blocks (ctx, 2, 59);   // the rest of the 4096-sample (16-block) crossfade, then KEMAR alone
    CHECK_FALSE (engine->isRendererCrossfadeActive());

    checkNothingAllocatedOrLocked (run.counts(), true);
    CHECK (run.allFinite);
}

// ----------------------------------------------------------------------------
// [rt][rule]: the lock and blocking-call inventory outside UI code is frozen. The pthread trap sees
// juce::CriticalSection and std::mutex (the canary proves it) but not juce::SpinLock, so a new lock
// that reaches the audio path through a spin lock, or one the trap's cases never run, would pass
// unseen. This scan lists every lock primitive and blocking call in non-UI source and headers; an
// addition, or a file listed here that no longer needs its entry, fails until reviewed.
// ----------------------------------------------------------------------------
namespace
{
    // Test-local copy of stripComments from tests/UI/FontProvenanceTests.cpp (that one lives in the UI
    // target's anonymous namespace; this target does not link it). Keeps code and string literals,
    // drops // and /* */ comments, keeps newlines so line numbers still match.
    std::string stripComments (const std::string& src)
    {
        std::string out;
        out.reserve (src.size());
        enum class State { code, lineComment, blockComment, string, character } state = State::code;

        for (size_t i = 0; i < src.size(); ++i)
        {
            const char c = src[i];
            const char next = i + 1 < src.size() ? src[i + 1] : '\0';

            switch (state)
            {
                case State::code:
                    if (c == '/' && next == '/')       { state = State::lineComment; ++i; }
                    else if (c == '/' && next == '*')  { state = State::blockComment; ++i; }
                    else
                    {
                        if (c == '"')  state = State::string;
                        if (c == '\'') state = State::character;
                        out += c;
                    }
                    break;
                case State::lineComment:
                    if (c == '\n') { state = State::code; out += c; }
                    break;
                case State::blockComment:
                    if (c == '*' && next == '/') { state = State::code; ++i; }
                    else if (c == '\n')          { out += c; }
                    break;
                case State::string:
                case State::character:
                    out += c;
                    if (c == '\\' && next != '\0') { out += next; ++i; }
                    else if ((state == State::string && c == '"') || (state == State::character && c == '\''))
                        state = State::code;
                    break;
            }
        }

        return out;
    }

    const std::regex& lockRule()
    {
        static const std::regex rule (
            "SpinLock|CriticalSection|ScopedLock|ScopedTryLock|ScopedReadLock|ScopedWriteLock"
            "|ReadWriteLock|std::mutex|recursive_mutex|shared_mutex|timed_mutex|lock_guard"
            "|unique_lock|scoped_lock|condition_variable|pthread_mutex_|pthread_rwlock_"
            "|pthread_cond_|os_unfair_lock|dispatch_semaphore|WaitableEvent"
            "|\\bwait *\\(|\\bnotify *\\(|sleep_for|Thread::sleep|startThread|stopThread");
        return rule;
    }

    struct LockHit
    {
        int line;
        std::string text;
    };

    // Every non-UI .h and .cpp under src/ and include/, comment-stripped, scanned line by line.
    std::map<std::string, std::vector<LockHit>> scanLockPrimitives()
    {
        std::map<std::string, std::vector<LockHit>> hits;
        // SPATIALCORE_SOURCE_DIR is "<repo>/tests/..", which JUCE does not collapse at the end of a
        // path; a child path is collapsed, so the repo root is the parent of the "src" child.
        const juce::File root = juce::File (SPATIALCORE_SOURCE_DIR).getChildFile ("src").getParentDirectory();

        for (const char* sub : { "src", "include" })
        {
            for (const juce::File& file : root.getChildFile (sub).findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
            {
                const std::string relative = file.getRelativePathFrom (root).replaceCharacter ('\\', '/').toStdString();

                if (relative.find ("/UI/") != std::string::npos || relative.rfind ("src/UI/", 0) == 0)
                    continue;

                std::istringstream lines (stripComments (file.loadFileAsString().toStdString()));
                std::string text;

                for (int line = 1; std::getline (lines, text); ++line)
                    if (std::regex_search (text, lockRule()))
                        hits[relative].push_back ({ line, text });
            }
        }

        return hits;
    }

    // The reviewed inventory: repo-relative path -> exact number of matching lines. Every entry names
    // the function and the thread it runs on. None is reachable from RenderEngine::renderBlock.
    const std::map<std::string, int>& reviewedLockInventory()
    {
        static const std::map<std::string, int> inventory = {
            // SharedFFTCache::lock, the process-global FFT cache's juce::SpinLock (the pthread trap
            // cannot see a SpinLock, which is why this rule exists). Taken only by getOrCreate,
            // below. PREPARE-TIME: PartitionedConvolver::prepare and HRTFDatabase::convertToMinPhase
            // (message thread, or the loader thread during a profile load). Never from renderBlock.
            { "include/SpatialCore/Binaural/SharedFFTCache.h", 1 },
            // SharedFFTCache::getOrCreate: the ScopedLockType on that SpinLock. PREPARE-TIME, as above.
            { "src/Binaural/PartitionedConvolver.cpp", 1 },
            // RenderEngine::sharedFolderLock_ (one juce::CriticalSection): guards the test-only shared
            // HRTF folder override. MESSAGE THREAD (setSharedHRTFFolderForTesting) and LOADER THREAD
            // (getSharedFolderForLoader). Never the render thread.
            { "include/SpatialCore/Engine/RenderEngine.h", 1 },
            // The 13 lines of RenderEngine.cpp, by function and thread:
            //   :89   HRTFProfileLoader::run, wait (200): idle wait.                     LOADER THREAD
            //   :238  HRTFProfileLoader, wait (pollMs): backoff while both renderers
            //         are busy (returns early on notify).                                LOADER THREAD
            //   :347 :348  ~RenderEngine, notify () and stopThread (15000).              MESSAGE THREAD
            //   :357  getSharedFolderForLoader, ScopedLock on sharedFolderLock_.         LOADER THREAD
            //   :363  setSharedHRTFFolderForTesting, ScopedLock on sharedFolderLock_.    MESSAGE THREAD
            //   :394 :396  setHRTFProfile, startThread and notify.                       MESSAGE THREAD
            //   :441  waitForHRTFProfileIdle, Thread::sleep (2) poll.                    MESSAGE THREAD
            //   :499 :500  prepare, notify () and stopThread (15000) before resizing.    MESSAGE THREAD
            //   :612 :613  prepare, startThread and notify after preparing.              MESSAGE THREAD
            { "src/Engine/RenderEngine.cpp", 13 },
        };

        return inventory;
    }
}

TEST_CASE ("Realtime: the comment stripper keeps code and drops prose", "[rt][rule]")
{
    CHECK (stripComments ("a(); // CriticalSection\nb();") == "a(); \nb();");
    CHECK (stripComments ("a(); /* juce::SpinLock */ b();") == "a();  b();");
    CHECK (stripComments ("x = \"http://SpinLock\";") == "x = \"http://SpinLock\";");   // // inside a string stays
    CHECK (stripComments ("c = '\"'; // tail") == "c = '\"'; ");
    CHECK (stripComments ("s = \"a\\\"//b\";") == "s = \"a\\\"//b\";");           // an escaped quote does not end the string
}

TEST_CASE ("Realtime: lock and blocking primitives outside UI code match the reviewed allow-list",
           "[rt][rule]")
{
    // Positive controls: the rule is live, so an empty scan would mean something.
    CHECK (std::regex_search ("const juce::ScopedLock l (x);", lockRule()));
    CHECK (std::regex_search ("std::lock_guard<std::mutex> g (m);", lockRule()));
    CHECK (std::regex_search ("juce::SpinLock::ScopedLockType l (lock);", lockRule()));
    CHECK (std::regex_search ("cv.wait (lock);", lockRule()));
    CHECK (std::regex_search ("juce::Thread::sleep (2);", lockRule()));
    // Negative control: a name that merely begins with "wait" is not a blocking call.
    CHECK_FALSE (std::regex_search ("engine.waitForHRTFProfileIdle (100);", lockRule()));

    const auto hits = scanLockPrimitives();
    const auto& inventory = reviewedLockInventory();

    for (const auto& [file, found] : hits)
    {
        const auto allowed = inventory.find (file);

        if (allowed == inventory.end() || (int) found.size() > allowed->second)
        {
            for (const LockHit& hit : found)
                FAIL_CHECK ("lock or blocking primitive not in the reviewed allow-list: " << file << ":" << hit.line
                            << ": " << hit.text
                            << "\n  Review which thread runs it (an audio-thread lock is a Phase 5 finding), then add the file"
                               " to reviewedLockInventory() with the function and thread.");
        }
    }

    for (const auto& [file, count] : inventory)
    {
        const auto found = hits.find (file);
        const int actual = found == hits.end() ? 0 : (int) found->second.size();

        if (actual < count)
            FAIL_CHECK ("stale allow-list entry: " << file << " lists " << count << " lines but the scan finds " << actual
                        << ". Lower the entry (or remove it) in reviewedLockInventory().");
    }
}
