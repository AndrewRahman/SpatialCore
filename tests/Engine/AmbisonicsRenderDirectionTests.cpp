#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>
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
// NFC choice: the engine runs a per-order NFC-HOA shelf on orders 1+ (renderAmbisonicsOutput).
// At DC that shelf has gain tan(wZero)/tan(wPole), about r / kNfcReferenceRadius (1.5 m), so a
// source at 1.5 m (distance 0.15 of the 0..10 m range) has pole == zero, a gain of 1, and the
// shelf is neutral. The engine smooths the distance toward its target and only rewrites the
// filter once it has moved 0.01 m, so the settled filter can sit up to 0.01 m / 1.5 m = 0.7 %
// off neutral; the ratio checks below carry a tolerance that covers it. Direction errors
// (a sign flip, a 90 degree rotation, a left/right swap) move a channel by 100 % or more.
// ============================================================================

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 64;
    constexpr int kSettleBlocks = 400;            // 25600 samples: many NFC and smoothing time constants
    constexpr float kNeutralNfcDistance = 0.15f;  // 1.5 m == kNfcReferenceRadius, shelf gain 1 at DC
    constexpr float kDcLevel = 0.5f;

    // "~ 0" in the cardinal-direction checks: |value| < kZeroRel * W.
    constexpr float kZeroRel = 1.0e-3f;

    // Ratio-to-W tolerance for the higher-order check: 2 % absolute on a ratio whose magnitude
    // is at most 1 (covers the NFC dead band above; a wrong sign or rotation is off by far more).
    constexpr float kRatioTol = 0.02f;

    int acnOrder (int acn)
    {
        int l = 0;
        while ((l + 1) * (l + 1) <= acn)
            ++l;
        return l;
    }

    // Max-rE weight of SH order n for an order-N stream, g_n = cos (n * pi / (2N + 2)). RenderEngine
    // keeps this in the private cachedMaxrE table, so the documented formula is reproduced here.
    float maxReWeight (int order, int totalOrder)
    {
        return std::cos (static_cast<float> (order) * juce::MathConstants<float>::pi
                         / (2.0f * static_cast<float> (totalOrder) + 2.0f));
    }

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
        sources.objects[0].distance = kNeutralNfcDistance;
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

    // Magnitude: sin(45) and cos(45) are equal, so |Z| and |X| match within the NFC dead band.
    CHECK (std::abs (out[(size_t) kZ] - out[(size_t) kX]) < kRatioTol * w);

    // Negative elevation flips Z only.
    const auto below = renderDc (OutputFormat::AmbisonicsFOA, 4, 0.0f, -45.0f);
    CHECK (below[(size_t) kZ] < 0.0f);
    CHECK (below[(size_t) kX] > 0.0f);
}

TEST_CASE ("Ambisonics render: every channel of a higher-order stream is evalSH x max-rE, relative to W",
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
                const float wRef = evalSH (0, azRad, elRad) * maxReWeight (0, f.order);

                for (int n = 1; n < numCh; ++n)
                {
                    const float expected = evalSH (n, azRad, elRad) * maxReWeight (acnOrder (n), f.order) / wRef;
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
