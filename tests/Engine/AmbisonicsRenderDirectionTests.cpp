#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>
#include "../reference/ShBackHalfReference.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace spatialcore;

// ============================================================================
// Ambisonics render direction, end to end through RenderEngine.
//
// One mono source is rendered on an Ambisonics output format and the channel values are checked
// against the AmbiX convention (ACN order, SN3D, no Condon-Shortley phase, azimuth 0 = front,
// positive azimuth = left; IO/AmbisonicsCodec.h). The wiring is the one RouteRenderRig uses
// (engineComputesGains + engineDerivesDispatch, positions in degrees, distance 0..1), except
// that the input is DC so a settled block gives one clean value per channel.
//
// The Ambisonics stream is plain AmbiX SN3D (SpatialCore#32): evalSH x signal x the block's
// distance gain x tap fade, with no near-field filter and no max-rE weights, so the channel
// ratios to W do not depend on the source distance. Direction errors (a sign flip, a 90 degree
// rotation, a left/right swap) move a channel by 100 % or more.
// ============================================================================

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 64;
    constexpr int kSettleBlocks = 400;            // 25600 samples, well past every smoothing time constant
    constexpr float kDefaultDistance = 0.15f;     // 1.5 m of the 0..10 m range
    constexpr float kDcLevel = 0.5f;

    // "~ 0" in the cardinal-direction checks: |value| < kZeroRel * W.
    constexpr float kZeroRel = 1.0e-3f;

    // Ratio-to-W tolerance: absolute, on a ratio whose magnitude is at most 1. The stream is exact
    // up to float rounding; a wrong sign or rotation is off by far more. The plain-AmbiX cases at
    // the end of the file use the same bound as kRatioTol.
    constexpr float kRatioTol = 1.0e-4f;

    // Renders a DC source at (azDeg, elDeg) for kSettleBlocks blocks on a fresh engine and returns
    // the mean of the last block for every output channel.
    std::vector<float> renderDc (OutputFormat format, int numChannels, float azDeg, float elDeg)
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (format);

        std::vector<float> mono ((size_t) kBlockSize, kDcLevel);
        std::vector<float> tapFade ((size_t) kBlockSize, 1.0f);
        std::vector<float> distGain ((size_t) kBlockSize, 1.0f);

        RenderSources sources;
        sources.numSamples = kBlockSize;
        sources.monoBuffers[0] = mono.data();
        sources.tapFadeGainPerSample[0] = tapFade.data();
        sources.distGainPerSample[0] = distGain.data();
        sources.objectLive[0] = true;
        sources.objects[0].azimuthDeg = azDeg;
        sources.objects[0].elevationDeg = elDeg;
        sources.objects[0].distance = kDefaultDistance;
        sources.objects[0].enabled = true;

        std::vector<std::vector<float>> channels ((size_t) numChannels, std::vector<float> ((size_t) kBlockSize, 0.0f));
        std::vector<float*> ptrs ((size_t) numChannels);
        for (int c = 0; c < numChannels; ++c)
            ptrs[(size_t) c] = channels[(size_t) c].data();

        for (int b = 0; b < kSettleBlocks; ++b)
        {
            RenderBlockContext ctx;
            ctx.sampleRate = kSampleRate;
            ctx.engineComputesGains = true;
            ctx.engineDerivesDispatch = true;
            engine.renderBlock (sources, ctx, ptrs.data(), numChannels);
        }

        std::vector<float> mean ((size_t) numChannels, 0.0f);
        for (int c = 0; c < numChannels; ++c)
        {
            double sum = 0.0;
            for (float s : channels[(size_t) c])
                sum += (double) s;
            mean[(size_t) c] = (float) (sum / (double) kBlockSize);
        }
        return mean;
    }

    // ACN 0..3 = W, Y, Z, X.
    constexpr int kW = 0, kY = 1, kZ = 2, kX = 3;
}

TEST_CASE ("Ambisonics render: first-order channels follow the AmbiX direction convention",
           "[ambi][engine][ambix]")
{
    struct Dir { float azDeg; float xSign; float ySign; const char* name; };
    // sign 0 means the channel must be ~ 0 (|value| < kZeroRel * W).
    const Dir dirs[] = {
        {    0.0f,  1.0f,  0.0f, "front, az 0"        },
        {   90.0f,  0.0f,  1.0f, "left, az +90"       },
        {  180.0f, -1.0f,  0.0f, "back, az 180"       },
        {  -90.0f,  0.0f, -1.0f, "right, az -90"      },
    };

    for (const auto& d : dirs)
    {
        DYNAMIC_SECTION (d.name)
        {
            const auto out = renderDc (OutputFormat::AmbisonicsFOA, 4, d.azDeg, 0.0f);
            const float w = out[(size_t) kW];

            CHECK (w > 0.0f);
            CHECK (std::abs (out[(size_t) kZ]) < kZeroRel * w);

            if (d.xSign > 0.0f)       CHECK (out[(size_t) kX] > 0.0f);
            else if (d.xSign < 0.0f)  CHECK (out[(size_t) kX] < 0.0f);
            else                      CHECK (std::abs (out[(size_t) kX]) < kZeroRel * w);

            if (d.ySign > 0.0f)       CHECK (out[(size_t) kY] > 0.0f);
            else if (d.ySign < 0.0f)  CHECK (out[(size_t) kY] < 0.0f);
            else                      CHECK (std::abs (out[(size_t) kY]) < kZeroRel * w);
        }
    }
}

TEST_CASE ("Ambisonics render: an elevated source (az 0, el +45) has positive Z and X",
           "[ambi][engine][ambix]")
{
    const auto out = renderDc (OutputFormat::AmbisonicsFOA, 4, 0.0f, 45.0f);
    const float w = out[(size_t) kW];

    CHECK (w > 0.0f);
    CHECK (out[(size_t) kZ] > 0.0f);
    CHECK (out[(size_t) kX] > 0.0f);
    CHECK (std::abs (out[(size_t) kY]) < kZeroRel * w);

    // Magnitude: sin(45) and cos(45) are equal, so |Z| and |X| match.
    CHECK (std::abs (out[(size_t) kZ] - out[(size_t) kX]) < kRatioTol * w);

    // Negative elevation flips Z only.
    const auto below = renderDc (OutputFormat::AmbisonicsFOA, 4, 0.0f, -45.0f);
    CHECK (below[(size_t) kZ] < 0.0f);
    CHECK (below[(size_t) kX] > 0.0f);
}

TEST_CASE ("Ambisonics render: every channel of a higher-order stream is plain evalSH, relative to W (no max-rE)",
           "[ambi][engine][ambix]")
{
    struct Fmt { OutputFormat format; int order; const char* name; };
    const Fmt formats[] = {
        { OutputFormat::AmbisonicsHOA,  3, "3rd order (16 ch)" },
        { OutputFormat::Ambisonics6OA,  6, "6th order (49 ch), the highest the registry offers" },
    };

    struct Dir { float azDeg; float elDeg; };
    const Dir dirs[] = { { 0.0f, 0.0f }, { 90.0f, 0.0f }, { 180.0f, 0.0f }, { -90.0f, 0.0f },
                         { 30.0f, 20.0f }, { -135.0f, -35.0f }, { 60.0f, 70.0f } };

    for (const auto& f : formats)
    {
        const int numCh = (f.order + 1) * (f.order + 1);

        for (const auto& d : dirs)
        {
            DYNAMIC_SECTION (f.name << ", az " << d.azDeg << " el " << d.elDeg)
            {
                const auto out = renderDc (f.format, numCh, d.azDeg, d.elDeg);
                const float w = out[(size_t) kW];
                REQUIRE (w > 0.0f);

                const float azRad = juce::degreesToRadians (d.azDeg);
                const float elRad = juce::degreesToRadians (d.elDeg);
                const float wRef = evalSH (0, azRad, elRad);

                for (int n = 1; n < numCh; ++n)
                {
                    const float expected = evalSH (n, azRad, elRad) / wRef;
                    const float got = out[(size_t) n] / w;

                    INFO ("ACN " << n << " expected ratio " << expected << " got " << got);
                    CHECK (std::abs (got - expected) < kRatioTol);

                    // The sign must match wherever the expected value is clear of the tolerance.
                    if (std::abs (expected) > 2.0f * kRatioTol)
                        CHECK ((got > 0.0f) == (expected > 0.0f));
                }
            }
        }
    }
}

// ============================================================================
// Plain AmbiX stream (SpatialCore#32). The Ambisonics output channels are evalSH x signal x the
// block's distance gain x tap fade, nothing else: no per-order near-field filter, no max-rE. The
// expected values come from the independent scipy tables (ShBackHalfReference.h), not from evalSH.
// ============================================================================
namespace
{
    constexpr float kResidualTol = 1.0e-3f;   // least-squares residual / channel RMS

    struct FitResult
    {
        std::vector<float> gain;       // per channel: least-squares gain onto the input
        std::vector<float> residual;   // per channel: RMS of what the gain cannot explain / channel RMS
    };

    // Renders a unit-amplitude input (DC when freqHz is 0, otherwise a sine continuous across blocks)
    // for kSettleBlocks blocks on a fresh engine, and over the last 100 blocks fits every output
    // channel to the input by least squares.
    FitResult renderFit (OutputFormat format, int numChannels, float azDeg, float elDeg,
                         float distance, double freqHz)
    {
        constexpr int kFitBlocks = 100;

        RenderEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setOutputFormat (format);

        std::vector<float> mono ((size_t) kBlockSize, 1.0f);
        std::vector<float> tapFade ((size_t) kBlockSize, 1.0f);
        std::vector<float> distGain ((size_t) kBlockSize, 1.0f);

        RenderSources sources;
        sources.numSamples = kBlockSize;
        sources.monoBuffers[0] = mono.data();
        sources.tapFadeGainPerSample[0] = tapFade.data();
        sources.distGainPerSample[0] = distGain.data();
        sources.objectLive[0] = true;
        sources.objects[0].azimuthDeg = azDeg;
        sources.objects[0].elevationDeg = elDeg;
        sources.objects[0].distance = distance;
        sources.objects[0].enabled = true;

        std::vector<std::vector<float>> channels ((size_t) numChannels, std::vector<float> ((size_t) kBlockSize, 0.0f));
        std::vector<float*> ptrs ((size_t) numChannels);
        for (int c = 0; c < numChannels; ++c)
            ptrs[(size_t) c] = channels[(size_t) c].data();

        std::vector<double> sxy ((size_t) numChannels, 0.0), syy ((size_t) numChannels, 0.0);
        double sxx = 0.0;
        double phase = 0.0;
        const double phaseStep = 2.0 * juce::MathConstants<double>::pi * freqHz / kSampleRate;

        for (int b = 0; b < kSettleBlocks; ++b)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                mono[(size_t) i] = (freqHz == 0.0) ? 1.0f : (float) std::sin (phase);
                phase += phaseStep;
            }

            RenderBlockContext ctx;
            ctx.sampleRate = kSampleRate;
            ctx.engineComputesGains = true;
            ctx.engineDerivesDispatch = true;
            engine.renderBlock (sources, ctx, ptrs.data(), numChannels);

            if (b >= kSettleBlocks - kFitBlocks)
            {
                for (int i = 0; i < kBlockSize; ++i)
                {
                    const double x = (double) mono[(size_t) i];
                    sxx += x * x;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const double y = (double) channels[(size_t) c][(size_t) i];
                        sxy[(size_t) c] += x * y;
                        syy[(size_t) c] += y * y;
                    }
                }
            }
        }

        FitResult r;
        r.gain.assign ((size_t) numChannels, 0.0f);
        r.residual.assign ((size_t) numChannels, 0.0f);
        for (int c = 0; c < numChannels; ++c)
        {
            const double g = sxy[(size_t) c] / sxx;
            const double resEnergy = std::max (0.0, syy[(size_t) c] - g * sxy[(size_t) c]);
            r.gain[(size_t) c] = (float) g;
            r.residual[(size_t) c] = (syy[(size_t) c] > 0.0) ? (float) std::sqrt (resEnergy / syy[(size_t) c]) : 0.0f;
        }
        return r;
    }

    struct StreamFormat { OutputFormat format; int order; const char* name; };
    const StreamFormat kStreamFormats[] = {
        { OutputFormat::AmbisonicsFOA, 1, "1st order (4 ch)"  },
        { OutputFormat::AmbisonicsHOA, 3, "3rd order (16 ch)" },
        { OutputFormat::Ambisonics6OA, 6, "6th order (49 ch)" },
    };

    const spatialcore_ref::BackHalfRef& backHalfRef (float azDeg, float elDeg)
    {
        for (const auto& ref : spatialcore_ref::kBackHalfRefs)
            if (ref.azDeg == azDeg && ref.elDeg == elDeg)
                return ref;
        FAIL ("no back-half reference at az " << azDeg << " el " << elDeg);
        return spatialcore_ref::kBackHalfRefs[0];
    }
}

TEST_CASE ("Ambisonics render: the output stream equals the scipy SN3D tables at orders 1, 3 and 6 (SpatialCore#32)",
           "[ambi][engine][ambix][plain-ambix]")
{
    for (const auto& f : kStreamFormats)
    {
        const int numCh = (f.order + 1) * (f.order + 1);

        for (const auto& ref : spatialcore_ref::kBackHalfRefs)
        {
            DYNAMIC_SECTION (f.name << ", az " << ref.azDeg << " el " << ref.elDeg)
            {
                const auto out = renderDc (f.format, numCh, ref.azDeg, ref.elDeg);
                REQUIRE (out[(size_t) kW] > 0.0f);

                for (int n = 0; n < numCh; ++n)
                {
                    INFO ("golden: " << f.name << ", az " << ref.azDeg << " el " << ref.elDeg << ", ACN " << n
                          << " expected " << ref.table[n] << " got " << out[(size_t) n] / kDcLevel);
                    CHECK (std::abs (out[(size_t) n] / kDcLevel - ref.table[n]) < kRatioTol);
                }
            }
        }
    }
}

TEST_CASE ("Ambisonics render: channel ratios to W do not depend on distance, at DC and at 50 Hz (SpatialCore#32)",
           "[ambi][engine][ambix][plain-ambix]")
{
    constexpr float kAz = -100.0f, kEl = -25.0f;
    const auto& ref = backHalfRef (kAz, kEl);
    const float distances[] = { 0.05f, 0.15f, 0.5f, 1.0f };      // 0.5, 1.5, 5 and 10 m
    const double freqs[] = { 0.0, 50.0 };

    for (const auto& f : kStreamFormats)
    {
        const int numCh = (f.order + 1) * (f.order + 1);

        for (const double freq : freqs)
        {
            DYNAMIC_SECTION (f.name << ", " << (freq == 0.0 ? "DC" : "50 Hz"))
            {
                const auto base = renderFit (f.format, numCh, kAz, kEl, 0.15f, freq);
                REQUIRE (base.gain[(size_t) kW] > 0.0f);

                for (const float dist : distances)
                {
                    const auto fit = renderFit (f.format, numCh, kAz, kEl, dist, freq);
                    REQUIRE (fit.gain[(size_t) kW] > 0.0f);

                    for (int n = 1; n < numCh; ++n)
                    {
                        const float ratio = fit.gain[(size_t) n] / fit.gain[(size_t) kW];
                        const float baseRatio = base.gain[(size_t) n] / base.gain[(size_t) kW];
                        const float expected = ref.table[n] / ref.table[kW];

                        INFO ("golden: " << f.name << ", " << freq << " Hz, distance " << dist
                              << ", ACN " << n << " expected " << expected << " got " << ratio);
                        CHECK (std::abs (ratio - expected) < kRatioTol);

                        INFO ("distance independence: " << f.name << ", " << freq << " Hz, distance " << dist
                              << ", ACN " << n << " ratio " << ratio << " vs ratio at 1.5 m " << baseRatio);
                        CHECK (std::abs (ratio - baseRatio) < kRatioTol);

                        INFO ("phase residual: " << f.name << ", " << freq << " Hz, distance " << dist
                              << ", ACN " << n << " residual " << fit.residual[(size_t) n]);
                        CHECK (fit.residual[(size_t) n] < kResidualTol);
                    }
                }
            }
        }
    }
}
