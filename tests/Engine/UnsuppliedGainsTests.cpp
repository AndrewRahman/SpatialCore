#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace spatialcore;

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
