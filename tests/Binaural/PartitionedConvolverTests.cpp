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

// ============================================================================
// Transition timing is sample-based (BUG-02). Three pins:
//   1. 512-sample calls, IR 256: identical to the original call-counted scheme (one warm-up
//      call, then a four-call fade on cos/sin quarter steps).
//   2. 32-sample calls, IR 256: warm-up is irLen samples, the fade is the 2048-sample floor.
//   3. 512-sample calls, IR 558 (> one call): warm-up lasts two calls. This is the one
//      stated change at 512 and above.
// Expected signals come from the double-precision oracle. The incoming slot starts empty at
// setIR(), so its output is the direct convolution of the input from the setIR point; every
// comparison below is made at least one IR length after that point, where it equals the
// convolution of the whole input.
// ============================================================================

TEST_CASE ("PartitionedConvolver: transition timing is sample-based and unchanged at 512",
           "[convolver][transition]")
{
    constexpr size_t kNumSamples = 16384;
    constexpr size_t kSwitchAt = 4096;            // a multiple of 32 and of 512
    constexpr double kHalfPi = 1.57079632679489661923;

    auto makeInput = [] { return whiteNoise (kNumSamples, 31, 0.5f); };

    SECTION ("512-sample calls, IR 256: one warm-up call, then a four-call cos/sin fade")
    {
        constexpr int kIRLength = 256;
        constexpr size_t kCall = 512;
        const std::vector<float> irA = whiteNoise (kIRLength, 32, 0.05f);
        const std::vector<float> irB = whiteNoise (kIRLength, 33, 0.05f);
        const std::vector<float> input = makeInput();
        const std::vector<double> convA = directConvolve (input, irA);
        const std::vector<double> convB = directConvolve (input, irB);

        PartitionedConvolver conv;
        conv.prepare (512, kIRLength);
        conv.setIR (irA.data(), kIRLength);

        std::vector<float> out (kNumSamples, 0.0f);
        convolveInChunks (conv, input, { 512 }, 0, kSwitchAt, &out);
        conv.setIR (irB.data(), kIRLength);
        convolveInChunks (conv, input, { 512 }, kSwitchAt, kNumSamples, &out);

        // Call 1 after setIR: warm-up, output is still the old IR.
        double worstWarmup = 0.0;
        for (size_t i = kSwitchAt; i < kSwitchAt + kCall; ++i)
            worstWarmup = std::max (worstWarmup, std::abs (static_cast<double> (out[i]) - convA[i]));
        INFO ("warm-up call vs conv(A): " << worstWarmup);
        CHECK (worstWarmup <= 2.0e-6);

        // Calls 2-5: per-sample gains ramp linearly between cos/sin of (k-1)/4 and k/4. The
        // expectation repeats the convolver's own float arithmetic (the gain is accumulated one
        // increment per sample, which drifts about 4e-6 from an exact double ramp at 512 samples)
        // so that this pins the timing and the gain law, not float rounding.
        double worstFade = 0.0;
        for (int k = 1; k <= 4; ++k)
        {
            const float halfPi = static_cast<float> (kHalfPi);
            float gOut = std::cos (static_cast<float> (k - 1) / 4.0f * halfPi);
            float gIn = std::sin (static_cast<float> (k - 1) / 4.0f * halfPi);
            const float outEnd = std::cos (static_cast<float> (k) / 4.0f * halfPi);
            const float inEnd = std::sin (static_cast<float> (k) / 4.0f * halfPi);
            const float outInc = (outEnd - gOut) / static_cast<float> (kCall);
            const float inInc = (inEnd - gIn) / static_cast<float> (kCall);
            const size_t base = kSwitchAt + static_cast<size_t> (k) * kCall;

            for (size_t i = 0; i < kCall; ++i)
            {
                gOut += outInc;
                gIn += inInc;
                const double expected = convA[base + i] * static_cast<double> (gOut)
                                      + convB[base + i] * static_cast<double> (gIn);
                worstFade = std::max (worstFade, std::abs (static_cast<double> (out[base + i]) - expected));
            }
        }
        INFO ("fade calls vs cos/sin expectation: " << worstFade);
        CHECK (worstFade <= 1.0e-6);

        // From call 6 on the output is the new IR alone.
        double worstAfter = 0.0;
        for (size_t i = kSwitchAt + 5 * kCall; i < kNumSamples; ++i)
            worstAfter = std::max (worstAfter, std::abs (static_cast<double> (out[i]) - convB[i]));
        INFO ("after the fade vs conv(B): " << worstAfter);
        CHECK (worstAfter <= 2.0e-6);
    }

    SECTION ("32-sample calls, IR 256: warm-up is irLen samples, the fade is the 2048-sample floor")
    {
        constexpr int kIRLength = 256;
        constexpr size_t kWarmup = 256;
        constexpr size_t kFade = 2048;
        const std::vector<float> irA = whiteNoise (kIRLength, 34, 0.05f);
        const std::vector<float> irB = whiteNoise (kIRLength, 35, 0.05f);
        const std::vector<float> input = makeInput();
        const std::vector<double> convA = directConvolve (input, irA);
        const std::vector<double> convB = directConvolve (input, irB);

        PartitionedConvolver conv;
        conv.prepare (512, kIRLength);
        conv.setIR (irA.data(), kIRLength);

        std::vector<float> out (kNumSamples, 0.0f);
        convolveInChunks (conv, input, { 32 }, 0, kSwitchAt, &out);
        conv.setIR (irB.data(), kIRLength);
        convolveInChunks (conv, input, { 32 }, kSwitchAt, kNumSamples, &out);

        double worstWarmup = 0.0;
        for (size_t i = kSwitchAt; i < kSwitchAt + kWarmup; ++i)
            worstWarmup = std::max (worstWarmup, std::abs (static_cast<double> (out[i]) - convA[i]));
        INFO ("first irLen samples vs conv(A): " << worstWarmup);
        CHECK (worstWarmup <= 2.0e-6);

        // Half way through the fade the output is neither IR alone.
        const size_t mid = kSwitchAt + kWarmup + kFade / 2;
        double midFromA = 0.0;
        double midFromB = 0.0;
        for (size_t i = mid; i < mid + 32; ++i)
        {
            midFromA = std::max (midFromA, std::abs (static_cast<double> (out[i]) - convA[i]));
            midFromB = std::max (midFromB, std::abs (static_cast<double> (out[i]) - convB[i]));
        }
        INFO ("mid-fade distance from conv(A): " << midFromA << ", from conv(B): " << midFromB);
        CHECK (midFromA > 1.0e-3);
        CHECK (midFromB > 1.0e-3);

        // The new IR alone from irLen + 2048 + 64 samples on.
        double worstAfter = 0.0;
        for (size_t i = kSwitchAt + kWarmup + kFade + 64; i < kNumSamples; ++i)
            worstAfter = std::max (worstAfter, std::abs (static_cast<double> (out[i]) - convB[i]));
        INFO ("after the fade vs conv(B): " << worstAfter);
        CHECK (worstAfter <= 2.0e-6);
    }

    SECTION ("512-sample calls, IR 558: warm-up lasts two calls")
    {
        constexpr int kIRLength = 558;
        constexpr size_t kCall = 512;
        const std::vector<float> irA = whiteNoise (kIRLength, 36, 0.05f);
        const std::vector<float> irB = whiteNoise (kIRLength, 37, 0.05f);
        const std::vector<float> input = makeInput();
        const std::vector<double> convA = directConvolve (input, irA);
        const std::vector<double> convB = directConvolve (input, irB);

        PartitionedConvolver conv;
        conv.prepare (512, kIRLength);
        conv.setIR (irA.data(), kIRLength);

        std::vector<float> out (kNumSamples, 0.0f);
        convolveInChunks (conv, input, { 512 }, 0, kSwitchAt, &out);
        conv.setIR (irB.data(), kIRLength);
        convolveInChunks (conv, input, { 512 }, kSwitchAt, kNumSamples, &out);

        // Calls 1 and 2: 512 samples is shorter than the 558-sample IR, so call 2 is still warm-up.
        double worstWarmup = 0.0;
        for (size_t i = kSwitchAt; i < kSwitchAt + 2 * kCall; ++i)
            worstWarmup = std::max (worstWarmup, std::abs (static_cast<double> (out[i]) - convA[i]));
        INFO ("two warm-up calls vs conv(A): " << worstWarmup);
        CHECK (worstWarmup <= 2.0e-6);

        // Call 3 is the first fade call: neither IR alone.
        double fromA = 0.0;
        for (size_t i = kSwitchAt + 2 * kCall; i < kSwitchAt + 3 * kCall; ++i)
            fromA = std::max (fromA, std::abs (static_cast<double> (out[i]) - convA[i]));
        INFO ("first fade call distance from conv(A): " << fromA);
        CHECK (fromA > 1.0e-3);

        // Four fade calls (3-6), then the new IR alone from call 7.
        double worstAfter = 0.0;
        for (size_t i = kSwitchAt + 6 * kCall; i < kNumSamples; ++i)
            worstAfter = std::max (worstAfter, std::abs (static_cast<double> (out[i]) - convB[i]));
        INFO ("after the fade vs conv(B): " << worstAfter);
        CHECK (worstAfter <= 2.0e-6);
    }
}

// ============================================================================
// D-12: in steady state the engine's output must not depend on how the host cuts the stream
// into blocks. Reference: the 512-sample plan; compared after 16384 samples, once gain ramps,
// HRIR loads and any transition have finished, on the Simple path and on the KEMAR HRTF path.
// ============================================================================

TEST_CASE ("BUG-02: engine output at small and irregular block plans matches the 512-sample reference",
           "[bug02][steady]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr size_t kNumSamples = 60000;
    constexpr size_t kSettle = 16384;
    const std::vector<float> input = whiteNoise (kNumSamples, 5, 0.25f);

    for (BinauralPath path : { BinauralPath::Simple, BinauralPath::HRTF })
    {
        const char* pathName = (path == BinauralPath::Simple) ? "Simple" : "HRTF/KEMAR";

        auto render = [&] (const std::vector<int>& plan)
        {
            RenderEngine engine;
            engine.prepare (kSampleRate, 512);
            engine.setOutputFormat (OutputFormat::Binaural);
            if (path == BinauralPath::HRTF)
                REQUIRE (loadProfileIntoActiveRenderer (engine, 5, kSampleRate));

            const RenderBlockContext ctx = makeBinauralContext (path, kSampleRate);
            return renderThroughEngine (engine, ctx, input, plan,
                                        [] (int64_t) { return Direction { 50.0f, 20.0f }; });
        };

        const StereoSignal reference = render ({ 512 });
        REQUIRE (signalIsFinite (reference));

        for (const auto& plan : blockPlans())
        {
            if (plan.size() == 1 && plan.front() == 512)
                continue;

            const StereoSignal other = render (plan);

            double worst = 0.0;
            int nonFinite = 0;
            for (size_t i = kSettle; i < kNumSamples; ++i)
            {
                if (! spatialcore_test::accumulateWorstFinite (worst, static_cast<double> (std::abs (other.left[i] - reference.left[i]))))
                    ++nonFinite;
                if (! spatialcore_test::accumulateWorstFinite (worst, static_cast<double> (std::abs (other.right[i] - reference.right[i]))))
                    ++nonFinite;
            }

            INFO ("path " << pathName << ", plan starts at " << plan.front() << " (" << plan.size()
                  << " entries): worst |diff| vs 512 = " << worst);
            WARN ("steady-state " << pathName << " plan[0]=" << plan.front() << " size " << plan.size()
                  << ": worst |diff| " << worst);
            CHECK (nonFinite == 0);
            CHECK (worst <= 1.0e-4);
        }
    }
}
