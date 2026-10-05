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
