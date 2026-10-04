#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include "BinauralMetrics.h"
#include "../TestNumerics.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// PartitionedConvolver — dedicated tests (EXTR-02, BUG-02 oracle, D-12).
//
// Each TEST_CASE builds its own convolver; nothing is shared between cases except
// the lock-protected process-global SharedFFTCache, so a ctest per-test-process run
// and one SpatialCoreTests run give the same verdict.
// ============================================================================

namespace
{
    // Runs `input` through the convolver in `plan`-sized chunks (the last chunk is clamped).
    std::vector<float> convolveInChunks (PartitionedConvolver& conv, const std::vector<float>& input,
                                          const std::vector<int>& plan, size_t startAt = 0,
                                          size_t endAt = static_cast<size_t> (-1),
                                          std::vector<float>* outBuffer = nullptr)
    {
        std::vector<float> ownOut;
        std::vector<float>& out = (outBuffer != nullptr) ? *outBuffer : ownOut;
        if (out.size() != input.size())
            out.assign (input.size(), 0.0f);

        endAt = std::min (endAt, input.size());
        size_t pos = startAt;
        size_t planIndex = 0;
        while (pos < endAt)
        {
            const size_t n = std::min (static_cast<size_t> (plan[planIndex % plan.size()]), endAt - pos);
            ++planIndex;
            conv.process (input.data() + pos, out.data() + pos, static_cast<int> (n));
            pos += n;
        }
        return out;
    }
}

TEST_CASE ("PartitionedConvolver: output matches a double-precision direct convolution at every block plan",
           "[convolver][blocksize]")
{
    constexpr int kIRLength = 558;
    constexpr size_t kNumSamples = 20000;

    // Signal scale matters: float FFT error grows with the output magnitude, and the 2e-6 bound
    // comes from 03-RESEARCH.md measurements taken at an output standard deviation of about 0.2
    // (IR sigma 0.03, input sigma 0.3). An IR amplitude of 0.05 against an input amplitude of 0.5
    // reproduces that scale; at IR amplitude 0.5 the same convolver reads 1.7e-6, which is the
    // same relative accuracy but leaves almost no margin under the bound.
    const std::vector<float> ir = whiteNoise (kIRLength, 7, 0.05f);
    const std::vector<float> input = whiteNoise (kNumSamples, 11, 0.5f);
    const std::vector<double> expected = directConvolve (input, ir);

    for (const auto& plan : blockPlans())
    {
        PartitionedConvolver conv;
        conv.prepare (512, kIRLength);
        conv.setIR (ir.data(), kIRLength);   // first IR: direct load, no crossfade

        const std::vector<float> out = convolveInChunks (conv, input, plan);

        double worst = 0.0;
        int nonFinite = 0;
        for (size_t i = 0; i < kNumSamples; ++i)
            if (! spatialcore_test::accumulateWorstFinite (worst, std::abs (static_cast<double> (out[i]) - expected[i])))
                ++nonFinite;

        INFO ("plan starts at " << plan.front() << ", " << plan.size() << " entries; worst |diff| = " << worst);
        CHECK (nonFinite == 0);
        CHECK (worst <= 2.0e-6);
    }
}

TEST_CASE ("PartitionedConvolver: an IR set during a transition is deferred, then applied",
           "[convolver][pending-ir]")
{
    constexpr int kIRLength = 256;
    constexpr size_t kNumSamples = 20000;
    const std::vector<int> plan { 37, 64, 1, 300 };

    // Same signal scale as the block-plan test above (IR amplitude 0.05, input amplitude 0.5).
    const std::vector<float> irA = whiteNoise (kIRLength, 21, 0.05f);
    const std::vector<float> irB = whiteNoise (kIRLength, 22, 0.05f);
    const std::vector<float> irC = whiteNoise (kIRLength, 23, 0.05f);
    const std::vector<float> input = whiteNoise (kNumSamples, 24, 0.5f);

    PartitionedConvolver conv;
    conv.prepare (512, kIRLength);
    conv.setIR (irA.data(), kIRLength);

    std::vector<float> out (kNumSamples, 0.0f);
    convolveInChunks (conv, input, plan, 0, 2048, &out);

    conv.setIR (irB.data(), kIRLength);          // starts a transition (Warmup)
    conv.process (input.data() + 2048, out.data() + 2048, 37);   // one block: Warmup moves on
    conv.setIR (irC.data(), kIRLength);          // mid-transition: must be deferred, not dropped

    convolveInChunks (conv, input, plan, 2085, kNumSamples, &out);

    const std::vector<double> expectedC = directConvolve (input, irC);
    const std::vector<double> expectedB = directConvolve (input, irB);

    double worstVsC = 0.0;
    double worstVsB = 0.0;
    int nonFinite = 0;
    for (size_t i = 12000; i < kNumSamples; ++i)
    {
        if (! spatialcore_test::accumulateWorstFinite (worstVsC, std::abs (static_cast<double> (out[i]) - expectedC[i])))
            ++nonFinite;
        if (! spatialcore_test::accumulateWorstFinite (worstVsB, std::abs (static_cast<double> (out[i]) - expectedB[i])))
            ++nonFinite;
    }

    INFO ("worst |diff| vs the last IR: " << worstVsC << ", vs the superseded IR: " << worstVsB);
    CHECK (nonFinite == 0);
    CHECK (worstVsC <= 2.0e-6);
    CHECK (worstVsB > 1.0e-3);   // proves C, not B, won
}

// ============================================================================
// BUG-02 (Spatial-Media-Lab/OpenSpatialDelay#234), engine level: a moving source must be as
// smooth at 32, 64 and 128-sample blocks as at 512. The convolver's overlap-add is correct at
// every block size ([convolver][blocksize]); what made small blocks step harder was its IR
// warm-up and crossfade being counted in calls, so a new HRIR faded in after 160 samples at
// 32-sample blocks while the IRs are 128-558 samples long. The bound (1.2x the 512-sample
// step) was set from the research prototype, which measured 1.00x at all four sizes.
// ============================================================================

TEST_CASE ("BUG-02: a moving source is as smooth at 32, 64 and 128-sample blocks as at 512",
           "[bug02][moving]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kSweepSeconds = 2.0;
    constexpr size_t kNumSamples = static_cast<size_t> (2.25 * kSampleRate);
    constexpr size_t kMeasureFrom = static_cast<size_t> (0.25 * kSampleRate);

    const std::vector<float> input = sineWave (kNumSamples, 440.0, kSampleRate, 0.5f);

    auto measure = [&] (int blockSize)
    {
        RenderEngine engine;
        engine.prepare (kSampleRate, 512);
        engine.setOutputFormat (OutputFormat::Binaural);
        REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kSampleRate));

        const RenderBlockContext ctx = makeBinauralContext (BinauralPath::HRTF, kSampleRate);
        const StereoSignal out = renderThroughEngine (
            engine, ctx, input, { blockSize },
            [&] (int64_t blockStart)
            {
                const double t = static_cast<double> (blockStart) / kSampleRate;
                const double fraction = std::min (std::max (t / kSweepSeconds, 0.0), 1.0);
                return Direction { static_cast<float> (-90.0 + 180.0 * fraction), 0.0f };
            });

        REQUIRE (signalIsFinite (out));
        return std::max (maxAbsStep (out.left, kMeasureFrom, kNumSamples),
                         maxAbsStep (out.right, kMeasureFrom, kNumSamples));
    };

    const double step512 = measure (512);
    const double step128 = measure (128);
    const double step64 = measure (64);
    const double step32 = measure (32);

    INFO ("largest sample-to-sample step: 512 -> " << step512 << ", 128 -> " << step128
          << ", 64 -> " << step64 << ", 32 -> " << step32);
    WARN ("moving-source max step: 512 " << step512 << ", 128 " << step128 << " (x" << step128 / step512
          << "), 64 " << step64 << " (x" << step64 / step512 << "), 32 " << step32 << " (x" << step32 / step512 << ")");

    REQUIRE (step512 > 0.0);
    CHECK (step128 <= 1.2 * step512);
    CHECK (step64 <= 1.2 * step512);
    CHECK (step32 <= 1.2 * step512);
}
