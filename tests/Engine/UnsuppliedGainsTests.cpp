#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include "../Support/RealtimeGuard.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// INTG-02 (Phase 6, D-05, D-06, SpatialCore#14): a consumer that leaves engineComputesGains off AND
// forgets to supply gains renders silence that nobody notices. RenderEngine counts such a block
// (getUnsuppliedGainsBlockCount()) and, in a Debug build, asserts on the first one. It never
// computes gains on the consumer's behalf, so the output of a misuse block is exactly what the
// consumer supplied: silence.
//
// The deliberate-misuse cases below trip that Debug assertion on purpose: JUCE logs one line each
// and continues (it only breaks under a debugger). Release builds only count. The verdicts are
// the counts and the exit codes, never the log lines.
// ============================================================================

namespace
{
    // The consumer-side wiring: per-object mono, tap-fade and distance-gain buffers, an output bank
    // wide enough for every format, and one call that renders N blocks. Nothing here is library code.
    struct GainsRig
    {
        static constexpr int kBlock = 64;
        static constexpr double kRate = 48000.0;
        static constexpr int kMaxChannels = 64;

        GainsRig()
        {
            for (int t = 0; t < MAX_SOURCES; ++t)
            {
                auto& m = mono[(size_t) t];
                m.resize ((size_t) kBlock);
                const double freq = 220.0 + 20.0 * (double) t;
                for (int n = 0; n < kBlock; ++n)
                    m[(size_t) n] = 0.25f * (float) std::sin (2.0 * 3.14159265358979323846 * freq * (double) n / kRate);

                tapFade[(size_t) t].assign ((size_t) kBlock, 1.0f);
                dist[(size_t) t].assign ((size_t) kBlock, 1.0f);
            }

            bank.resize ((size_t) kMaxChannels);
            for (int c = 0; c < kMaxChannels; ++c)
            {
                bank[(size_t) c].assign ((size_t) kBlock, 0.0f);
                ptrs[c] = bank[(size_t) c].data();
            }
        }

        void prepare (OutputFormat format)
        {
            engine.prepare (kRate, kBlock);
            engine.setOutputFormat (format);
            numOutCh = OutputFormatRegistry::getInfo (format).requiredChannels;
        }

        // Objects 0..liveCount-1 live and enabled; every other slot not live, with null pointers.
        RenderSources sources (int liveCount) const
        {
            RenderSources s;
            s.numSamples = kBlock;
            for (int t = 0; t < liveCount; ++t)
            {
                s.monoBuffers[t] = mono[(size_t) t].data();
                s.tapFadeGainPerSample[t] = tapFade[(size_t) t].data();
                s.distGainPerSample[t] = dist[(size_t) t].data();
                s.sourceEnabledBlock[t] = true;
                s.objectLive[t] = true;
                s.objects[t].azimuthDeg = 30.0f * (float) t - 15.0f;
                s.objects[t].elevationDeg = 0.0f;
                s.objects[t].distance = 1.0f;
                s.objects[t].enabled = true;
            }
            return s;
        }

        void clearOutput()
        {
            for (auto& ch : bank)
                std::fill (ch.begin(), ch.end(), 0.0f);
        }

        void render (const RenderSources& s, const RenderBlockContext& ctx, int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                clearOutput();
                engine.renderBlock (s, ctx, ptrs, numOutCh);
            }
        }

        bool outputIsExactlySilent() const
        {
            for (const auto& ch : bank)
                for (float v : ch)
                    if (v != 0.0f)
                        return false;
            return true;
        }

        // Discrete surround: the speaker channels only. The LFE channel is generated from the
        // mono sum, not from the per-speaker gains, so it carries signal on a misuse block too.
        bool speakerChannelsExactlySilent() const
        {
            const auto& layout = engine.getBlockLayout().layout;
            for (int sp = 0; sp < layout.numSpeakers; ++sp)
            {
                const int ch = layout.speakers[sp].channelIndex;
                if (ch < 0 || ch >= kMaxChannels)
                    continue;
                for (float v : bank[(size_t) ch])
                    if (v != 0.0f)
                        return false;
            }
            return true;
        }

        bool outputHasSignal() const
        {
            for (const auto& ch : bank)
                for (float v : ch)
                    if (std::abs (v) > 1.0e-6f)
                        return true;
            return false;
        }

        RenderEngine engine;
        int numOutCh = 2;
        std::array<std::vector<float>, MAX_SOURCES> mono, tapFade, dist;
        std::vector<std::vector<float>> bank;
        float* ptrs[kMaxChannels] = {};
    };

    // The stereo-misuse context: the engine derives the dispatch, the consumer computes nothing.
    RenderBlockContext stereoContext()
    {
        RenderBlockContext ctx;
        ctx.sampleRate = GainsRig::kRate;
        ctx.engineDerivesDispatch = true;
        return ctx;
    }
}

TEST_CASE ("INTG-02: a stereo block with live objects and no gains is counted, and nothing fills the gains",
           "[intg02]")
{
    SECTION ("a freshly prepared engine reports 0")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("flag false, all-zero gains: every block counted, output exactly silent")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (2);
        RenderBlockContext ctx = stereoContext();   // objGainL / objGainR all 0

        rig.render (s, ctx, 4);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 4);
        CHECK (rig.outputIsExactlySilent());
    }

    SECTION ("engineComputesGains true: not counted, and the output is not silent")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (2);
        RenderBlockContext ctx = stereoContext();
        ctx.engineComputesGains = true;

        rig.render (s, ctx, 4);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
        CHECK (rig.outputHasSignal());
    }

    SECTION ("gains supplied for objects 0 and 1, flag false: not counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (2);
        RenderBlockContext ctx = stereoContext();
        for (int t = 0; t < 2; ++t)
        {
            ctx.objGainL[t] = 0.5f;
            ctx.objGainR[t] = 0.5f;
        }

        rig.render (s, ctx, 4);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
        CHECK (rig.outputHasSignal());
    }
}

TEST_CASE ("INTG-02: discrete surround counts zero objChannelGains, and any supplied gain clears it",
           "[intg02]")
{
    RenderBlockContext ctx;
    ctx.sampleRate = GainsRig::kRate;
    ctx.engineDerivesDispatch = true;   // Surround7_1_4 has no consumer-set dispatch fields

    SECTION ("live objects 0-2 with zero objChannelGains: every block counted, speaker channels exactly silent")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Surround7_1_4);
        const RenderSources s = rig.sources (3);

        rig.render (s, ctx, 4);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 4);
        CHECK (rig.speakerChannelsExactlySilent());   // the engine did not fill the gains
    }

    SECTION ("object 1 given 0.3 on speaker 0 only: the consumer chose the supplied mode, count 0")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Surround7_1_4);
        const RenderSources s = rig.sources (3);
        ctx.objChannelGains[1][0] = 0.3f;

        rig.render (s, ctx, 4);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
        CHECK (rig.outputHasSignal());
    }

    SECTION ("engineComputesGains true: count 0")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Surround7_1_4);
        const RenderSources s = rig.sources (3);
        ctx.engineComputesGains = true;

        rig.render (s, ctx, 4);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
        CHECK (rig.outputHasSignal());
    }

    SECTION ("a gain past the layout's speakers is not read by the path, so it does not clear the count")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Quad);   // 4 speakers
        const RenderSources s = rig.sources (2);
        ctx.objChannelGains[0][MAX_SPEAKERS - 1] = 0.9f;   // beyond numSpeakers: never rendered

        rig.render (s, ctx, 2);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 2);
        CHECK (rig.speakerChannelsExactlySilent());
    }
}

TEST_CASE ("INTG-02: the Simple binaural path counts zero objGains, and engineSelectsHRTF's fill is not misuse",
           "[intg02]")
{
    // Dispatch fields set by hand, as a consumer that does not derive them would.
    RenderBlockContext ctx;
    ctx.sampleRate = GainsRig::kRate;
    ctx.activeFormat = OutputFormat::Binaural;
    ctx.isBinaural = true;
    ctx.isStereoVariant = false;
    ctx.isAmbiOutput = false;
    ctx.useHRTF = false;

    SECTION ("Simple path, zero objGains: counted every block, output exactly silent")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Binaural);
        const RenderSources s = rig.sources (2);

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 3);
        CHECK (rig.outputIsExactlySilent());
    }

    SECTION ("Simple path, gains supplied on one object: count 0")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Binaural);
        const RenderSources s = rig.sources (2);
        ctx.objGains[0].leftGain = 0.7f;
        ctx.objGains[0].rightGain = 0.3f;

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("engineSelectsHRTF with the same zero objGains: the engine fills them (WR-06), count 0")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Binaural);
        const RenderSources s = rig.sources (2);
        ctx.engineSelectsHRTF = true;

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
        CHECK (rig.outputHasSignal());
    }

    SECTION ("the direct-binaural HRTF path reads no consumer gains: zero gains count 0")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Binaural);
        const RenderSources s = rig.sources (2);
        ctx.useHRTF = true;

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }
}

TEST_CASE ("INTG-02: Ambisonics reads no consumer gains, so zero gains are never counted", "[intg02]")
{
    GainsRig rig;
    rig.prepare (OutputFormat::AmbisonicsFOA);
    const RenderSources s = rig.sources (2);
    RenderBlockContext ctx;
    ctx.sampleRate = GainsRig::kRate;
    ctx.engineDerivesDispatch = true;   // ambiOrder and isAmbiOutput come from the engine's layout

    rig.render (s, ctx, 3);

    CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    CHECK (rig.outputHasSignal());
}

TEST_CASE ("INTG-02: the detector judges the path the engine renders, not the consumer's stale dispatch fields",
           "[intg02]")
{
    GainsRig rig;
    rig.prepare (OutputFormat::Stereo);
    const RenderSources s = rig.sources (2);

    RenderBlockContext ctx = stereoContext();
    // The consumer's own copy of the format is stale: it still says HRTF binaural. With
    // engineDerivesDispatch these fields are ignored and the engine renders the stereo variant.
    ctx.isBinaural = true;
    ctx.useHRTF = true;
    ctx.isStereoVariant = false;

    rig.render (s, ctx, 3);

    CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 3);
    CHECK (rig.outputIsExactlySilent());
}

TEST_CASE ("INTG-02: edges of 'active' and of the block", "[intg02]")
{
    RenderBlockContext ctx = stereoContext();   // zero stereo gains on every row below

    SECTION ("no object live: never counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (0);

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("live objects whose tap fade is exactly 0 at both ends: not active, not counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        RenderSources s = rig.sources (2);
        std::vector<float> zeros ((size_t) GainsRig::kBlock, 0.0f);
        s.tapFadeGainPerSample[0] = zeros.data();
        s.tapFadeGainPerSample[1] = zeros.data();

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("a tap fade ramping 0 to 1 (0 at the first sample, above 0 at the last): active, counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        RenderSources s = rig.sources (1);
        std::vector<float> ramp ((size_t) GainsRig::kBlock);
        for (int n = 0; n < GainsRig::kBlock; ++n)
            ramp[(size_t) n] = (float) n / (float) (GainsRig::kBlock - 1);
        REQUIRE (ramp.front() == 0.0f);
        s.tapFadeGainPerSample[0] = ramp.data();

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 3);
    }

    SECTION ("a tap fade ramping 1 to 0 (above 0 at the first sample, 0 at the last): active, counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        RenderSources s = rig.sources (1);
        std::vector<float> ramp ((size_t) GainsRig::kBlock);
        for (int n = 0; n < GainsRig::kBlock; ++n)
            ramp[(size_t) n] = 1.0f - (float) n / (float) (GainsRig::kBlock - 1);
        REQUIRE (ramp.back() == 0.0f);
        s.tapFadeGainPerSample[0] = ramp.data();

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 3);
    }

    SECTION ("live objects with null mono buffers: not active, not counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        RenderSources s = rig.sources (2);
        s.monoBuffers[0] = nullptr;
        s.monoBuffers[1] = nullptr;

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("live objects with null tap-fade buffers: not active, not counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        RenderSources s = rig.sources (2);
        s.tapFadeGainPerSample[0] = nullptr;
        s.tapFadeGainPerSample[1] = nullptr;

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("one supplied active object beside zero-gain ones: not counted")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (4);
        ctx.objGainL[2] = 0.6f;
        ctx.objGainR[2] = 0.4f;

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 0);
    }

    SECTION ("a gain on a slot that is not live does not clear the count")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (2);
        ctx.objGainL[9] = 0.5f;   // slot 9 is not live

        rig.render (s, ctx, 3);

        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 3);
    }

    SECTION ("a zero-length block between misuse blocks adds nothing")
    {
        GainsRig rig;
        rig.prepare (OutputFormat::Stereo);
        const RenderSources s = rig.sources (2);
        RenderSources empty = rig.sources (2);
        empty.numSamples = 0;

        rig.render (s, ctx, 2);
        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 2);

        rig.render (empty, ctx, 3);
        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 2);

        rig.render (s, ctx, 1);
        CHECK (rig.engine.getUnsuppliedGainsBlockCount() == 3);
    }
}

TEST_CASE ("INTG-02: a misuse block allocates and locks nothing on the render thread", "[rt][intg02]")
{
#if JUCE_DEBUG
    SKIP ("the Debug assertion logs by design; the Release run is the proof");
#else
    requireRealtimeTrapCanary();

    constexpr int kBlocks = 120;
    auto rig = std::make_unique<GainsRig>();
    rig->prepare (OutputFormat::Stereo);
    const RenderSources s = rig->sources (2);
    const RenderBlockContext ctx = stereoContext();   // the misuse: nothing computed, nothing supplied

    resetRealtimeCounts();

    for (int b = 0; b < kBlocks; ++b)
    {
        rig->clearOutput();   // disarmed
        {
            const ScopedRealtimeTrap trap;
            rig->engine.renderBlock (s, ctx, rig->ptrs, rig->numOutCh);
        }
    }

    const RealtimeCounts counts = realtimeCounts();
    if (counts.allocs != 0)
        printFirstOffender();

    CAPTURE (counts.allocs, counts.frees, counts.locks);
    CHECK (counts.allocs == 0);
    CHECK (counts.frees == 0);
    CHECK (counts.locks == 0);
    CHECK (rig->engine.getUnsuppliedGainsBlockCount() == kBlocks);
#endif
}
