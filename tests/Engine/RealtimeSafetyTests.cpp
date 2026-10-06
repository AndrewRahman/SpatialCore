#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
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
