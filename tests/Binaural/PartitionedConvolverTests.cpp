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
// Regression test for Spatial-Media-Lab/OpenSpatialDelay#50: the HRTF transition that clicked
// when a new IR replaced the playing one. OpenSpatialDelay v1.0.5 fixed it by loading the new IR
// into the second convolver slot, warming that slot up and crossfading from the old slot to it.
//
// The input is a steady 440 Hz sine. IR A passes it unchanged (tap 0); IR B delays it by 27
// samples, a quarter period at 440 Hz and 48 kHz, so the two outputs differ by up to about 0.57
// while an equal-power fade between them neither dips nor bumps. A hard swap of the IR into the
// playing slot jumps straight from one to the other, which is the click. What each check proves:
//   - through <= 1.5 x steady: no sample step in the change window is larger than the sine's own
//     steady-state step, so there is no click.
//   - the minimum 256-sample RMS stays above 0.8 x the steady RMS: the fade does not dip.
//   - after the fade the output is a direct convolution with IR B (2e-6), and it is not IR A's
//     output: the change really happened.
// Both call sizes are checked because the fade length depends on the call size (BUG-02). A hard
// swap into the playing slot turns the step check red (D-17; the measured ratio of that red run is
// recorded in the 06-03 SUMMARY).
// ============================================================================

TEST_CASE ("PartitionedConvolver: an IR change mid-stream is click-free (Spatial-Media-Lab/OpenSpatialDelay#50 dual slots)",
           "[convolver][osd50]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr size_t kNumSamples = 24576;
    constexpr size_t kChangeAt = 8192;            // a multiple of 64 and of 512
    constexpr int kIRLength = 64;
    constexpr int kQuarterPeriod = 27;            // samples: 48000 / 440 / 4 = 27.3
    constexpr size_t kFadeFloor = 2048;           // settle margin in samples: a safe upper bound on the crossfade (kHRIRCrossfadeMs is 1024 samples at 48 kHz)

    const std::vector<float> input = sineWave (kNumSamples, 440.0, kSampleRate, 0.5f);

    std::vector<float> irA (static_cast<size_t> (kIRLength), 0.0f);
    std::vector<float> irB (static_cast<size_t> (kIRLength), 0.0f);
    irA[0] = 0.8f;
    irB[static_cast<size_t> (kQuarterPeriod)] = 0.8f;

    const std::vector<double> convA = directConvolve (input, irA);
    const std::vector<double> convB = directConvolve (input, irB);

    auto run = [&] (size_t callSize)
    {
        PartitionedConvolver conv;
        conv.prepare (static_cast<int> (callSize), kIRLength);
        conv.setIR (irA.data(), kIRLength);

        std::vector<float> out (kNumSamples, 0.0f);
        convolveInChunks (conv, input, { static_cast<int> (callSize) }, 0, kChangeAt, &out);
        conv.setIR (irB.data(), kIRLength);
        convolveInChunks (conv, input, { static_cast<int> (callSize) }, kChangeAt, kNumSamples, &out);

        const double steady = maxAbsStep (out, 2048, kChangeAt);
        const double through = maxAbsStep (out, kChangeAt - callSize, kChangeAt + 8192);
        const double steadyRms = windowRms (out, 2048, kChangeAt);
        const double lowestRms = minWindowRms (out, kChangeAt, kChangeAt + 8192, 256);

        // The new IR alone from the end of warm-up (one call) plus the fade, with two calls of margin.
        const size_t settled = kChangeAt + static_cast<size_t> (kIRLength) + kFadeFloor + 2 * callSize;
        double worstVsB = 0.0;
        double worstVsA = 0.0;
        int nonFinite = 0;
        for (size_t i = 0; i < kNumSamples; ++i)
        {
            if (! std::isfinite (out[i]))
                ++nonFinite;
            if (i >= settled)
            {
                spatialcore_test::accumulateWorstFinite (worstVsB, std::abs (static_cast<double> (out[i]) - convB[i]));
                worstVsA = std::max (worstVsA, std::abs (static_cast<double> (out[i]) - convA[i]));
            }
        }

        CAPTURE (callSize, steady, through, through / steady, steadyRms, lowestRms, lowestRms / steadyRms,
                 worstVsB, worstVsA);
        WARN ("Spatial-Media-Lab/OpenSpatialDelay#50 " << callSize << "-sample calls: steady step " << steady << ", step through the change "
              << through << " (x" << through / steady << " of steady), min 256-sample RMS " << lowestRms
              << " (x" << lowestRms / steadyRms << " of steady), worst |diff| vs IR B " << worstVsB
              << ", max |diff| vs IR A " << worstVsA);

        REQUIRE (steady > 0.0);
        CHECK (nonFinite == 0);
        CHECK (through <= 1.5 * steady);
        CHECK (lowestRms >= 0.8 * steadyRms);
        CHECK (worstVsB <= 2.0e-6);
        CHECK (worstVsA > 1.0e-3);   // the output really changed from IR A to IR B
    };

    SECTION ("64-sample calls")  { run (64); }
    SECTION ("512-sample calls") { run (512); }
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
// Transition timing is counted in samples from TransitionTiming.h, and the gain law is the
// correlation-compensated crossfade (Spatial-Media-Lab/OpenSpatialDelay#234, O211 / O212).
//
// This replaces the Phase 3 BUG-02 pins of the call-counted rule (one warm-up call and at
// least irLen samples, then max (4 calls, 2048 samples) on per-call cos/sin endpoints). That
// rule still tied the fade length to the host block at 1024 and above, which OSD's Spatial-Media-Lab/OpenSpatialDelay#234
// work removed: the warm-up is max (irLen, kHRIRWarmupMinMs) samples and the fade is
// kHRIRCrossfadeMs (1024 samples at 48 kHz) at every call size, and both may begin or end
// mid-call. The fade gains are a = 1 - t, b = t, g = 1 / sqrt (a^2 + b^2 + 2 rho a b), with
// rho the zero-lag correlation of the two slots' IRs (O211's fixed equal-power law gave a
// +3 dB bump between coherent neighbouring HRIRs, heard as 23-37 Hz pulsing).
// Pins:
//   1. IR 256, call sizes 512, 32 and an irregular plan: 256 warm-up samples of the old IR,
//      then 1024 fade samples that match the expected gains, then the new IR alone.
//   2. IR 558, 512-sample calls: the warm-up is 558 samples, so the fade starts mid-call.
//   3. Two coherent IRs (rho near 1): the mid-fade level does not rise (no +3 dB bump).
// Expected signals come from the double-precision oracle. The incoming slot starts empty at
// setIR(), so its output is the direct convolution of the input from the setIR point; every
// comparison below is made at least one IR length after that point.
// ============================================================================

namespace
{
    struct TransitionRun
    {
        std::vector<float> out;
        float rho = 0.0f;
    };

    TransitionRun runTransition (const std::vector<float>& irA, const std::vector<float>& irB,
                                 const std::vector<float>& input, const std::vector<int>& plan,
                                 size_t switchAt)
    {
        const int irLength = static_cast<int> (irA.size());
        PartitionedConvolver conv;
        conv.prepare (512, irLength, 48000.0);
        conv.setIR (irA.data(), irLength);

        TransitionRun run;
        run.out.assign (input.size(), 0.0f);
        convolveInChunks (conv, input, plan, 0, switchAt, &run.out);
        conv.setIR (irB.data(), irLength);
        run.rho = conv.getCrossfadeRho();
        convolveInChunks (conv, input, plan, switchAt, input.size(), &run.out);
        return run;
    }

    // Worst |out - expected| over [from, to), expected = gOut(k) conv(A) + gIn(k) conv(B) inside
    // the fade, conv(A) before it and conv(B) after it.
    double worstVsLaw (const TransitionRun& run, const std::vector<double>& convA,
                       const std::vector<double>& convB, size_t fadeStart, int fadeLength,
                       size_t from, size_t to)
    {
        double worst = 0.0;
        for (size_t i = from; i < to; ++i)
        {
            double expected = 0.0;
            if (i < fadeStart)
                expected = convA[i];
            else if (i >= fadeStart + static_cast<size_t> (fadeLength))
                expected = convB[i];
            else
            {
                float gOut = 0.0f, gIn = 0.0f;
                const int k = static_cast<int> (i - fadeStart) + 1;   // 1..fadeLength
                PartitionedConvolver::computeCrossfadeGains (k, fadeLength, run.rho, gOut, gIn);
                expected = convA[i] * static_cast<double> (gOut) + convB[i] * static_cast<double> (gIn);
            }
            worst = std::max (worst, std::abs (static_cast<double> (run.out[i]) - expected));
        }
        return worst;
    }
}

TEST_CASE ("PartitionedConvolver: transition timing is counted in samples at every call size, with the correlation-compensated law",
           "[convolver][transition]")
{
    // static: makeInput below has no capture list, and MSVC (19.51) rejects a local constexpr used in it
    // (C3493) that GCC and Clang accept; a static local needs no capture and has the same value.
    static constexpr size_t kNumSamples = 16384;
    constexpr size_t kSwitchAt = 4096;            // a multiple of 32 and of 512
    const int kFade = hrirTransitionMsToSamples (48000.0, kHRIRCrossfadeMs);
    const int kWarmupFloor = hrirTransitionMsToSamples (48000.0, kHRIRWarmupMinMs);
    REQUIRE (kFade == 1024);
    REQUIRE (kWarmupFloor == 256);

    const std::vector<float> input = whiteNoise (kNumSamples, 31, 0.5f);

    SECTION ("IR 256: 256 warm-up samples, then a 1024-sample fade, at 512, 32 and irregular calls")
    {
        constexpr int kIRLength = 256;
        const std::vector<float> irA = whiteNoise (kIRLength, 32, 0.05f);
        const std::vector<float> irB = whiteNoise (kIRLength, 33, 0.05f);
        const std::vector<double> convA = directConvolve (input, irA);
        const std::vector<double> convB = directConvolve (input, irB);
        const size_t fadeStart = kSwitchAt + static_cast<size_t> (std::max (kIRLength, kWarmupFloor));

        std::vector<float> reference;
        for (const std::vector<int>& plan : { std::vector<int> { 512 }, std::vector<int> { 32 },
                                              std::vector<int> { 37, 64, 1, 300 } })
        {
            // convolveInChunks clamps the last chunk at kSwitchAt, so every plan starts its
            // transition on the same sample.
            const TransitionRun run = runTransition (irA, irB, input, plan, kSwitchAt);
            INFO ("plan starts at " << plan.front() << ", rho " << run.rho);
            CHECK (run.rho >= 0.0f);
            CHECK (run.rho < 0.2f);   // independent noise IRs: near-uncorrelated

            const double worst = worstVsLaw (run, convA, convB, fadeStart, kFade, kSwitchAt, kNumSamples);
            INFO ("worst |diff| vs warm-up / law / new IR: " << worst);
            CHECK (worst <= 2.0e-6);

            // Half way through the fade the output is neither IR alone.
            const size_t mid = fadeStart + static_cast<size_t> (kFade / 2);
            double midFromA = 0.0, midFromB = 0.0;
            for (size_t i = mid; i < mid + 32; ++i)
            {
                midFromA = std::max (midFromA, std::abs (static_cast<double> (run.out[i]) - convA[i]));
                midFromB = std::max (midFromB, std::abs (static_cast<double> (run.out[i]) - convB[i]));
            }
            CHECK (midFromA > 1.0e-3);
            CHECK (midFromB > 1.0e-3);

            // The same transition at every call size, sample for sample.
            if (reference.empty())
                reference = run.out;
            else
            {
                double vsReference = 0.0;
                for (size_t i = kSwitchAt; i < kNumSamples; ++i)
                    vsReference = std::max (vsReference, static_cast<double> (std::abs (run.out[i] - reference[i])));
                INFO ("vs the 512-sample plan: " << vsReference);
                CHECK (vsReference <= 2.0e-6);
            }
        }
    }

    SECTION ("IR 558, 512-sample calls: the warm-up is 558 samples, so the fade starts mid-call")
    {
        constexpr int kIRLength = 558;
        const std::vector<float> irA = whiteNoise (kIRLength, 36, 0.05f);
        const std::vector<float> irB = whiteNoise (kIRLength, 37, 0.05f);
        const std::vector<double> convA = directConvolve (input, irA);
        const std::vector<double> convB = directConvolve (input, irB);

        const TransitionRun run = runTransition (irA, irB, input, { 512 }, kSwitchAt);
        const size_t fadeStart = kSwitchAt + kIRLength;   // 46 samples into the second call

        const double worst = worstVsLaw (run, convA, convB, fadeStart, kFade, kSwitchAt, kNumSamples);
        INFO ("worst |diff| vs warm-up / law / new IR: " << worst);
        CHECK (worst <= 2.0e-6);

        // The sample after the warm-up is already a fade sample.
        CHECK (std::abs (static_cast<double> (run.out[fadeStart + 8]) - convA[fadeStart + 8]) > 1.0e-6);
    }

    SECTION ("coherent IRs (rho near 1): the mid-fade level does not rise")
    {
        constexpr int kIRLength = 256;
        const std::vector<float> irA = whiteNoise (kIRLength, 38, 0.05f);
        const std::vector<float> wobble = whiteNoise (kIRLength, 39, 0.005f);
        std::vector<float> irB (irA);
        for (size_t i = 0; i < irB.size(); ++i)
            irB[i] += wobble[i];
        const std::vector<double> convA = directConvolve (input, irA);
        const std::vector<double> convB = directConvolve (input, irB);

        const TransitionRun run = runTransition (irA, irB, input, { 64 }, kSwitchAt);
        INFO ("rho " << run.rho);
        CHECK (run.rho > 0.98f);

        const size_t fadeStart = kSwitchAt + kIRLength;
        CHECK (worstVsLaw (run, convA, convB, fadeStart, kFade, kSwitchAt, kNumSamples) <= 2.0e-6);

        auto rms = [] (const std::vector<float>& x, size_t from, size_t n)
        {
            double acc = 0.0;
            for (size_t i = from; i < from + n; ++i)
                acc += static_cast<double> (x[i]) * static_cast<double> (x[i]);
            return std::sqrt (acc / static_cast<double> (n));
        };
        const size_t window = 256;
        const double before = rms (run.out, kSwitchAt - 2 * window, window);
        const double mid = rms (run.out, fadeStart + static_cast<size_t> (kFade / 2) - window / 2, window);
        const double riseDb = 20.0 * std::log10 (mid / before);
        INFO ("mid-fade level vs before: " << riseDb << " dB (O211 equal-power: about +3 dB)");
        CHECK (std::abs (riseDb) < 0.5);

        // The law itself: at rho 1 the mid-fade gains are 0.5 / 0.5 (equal-gain, no bump), at
        // rho 0 they are 1 / sqrt (2) each (constant power for uncorrelated outputs).
        float gOut = 0.0f, gIn = 0.0f;
        PartitionedConvolver::computeCrossfadeGains (512, 1024, 1.0f, gOut, gIn);
        CHECK (std::abs (gOut - 0.5f) < 1.0e-6f);
        CHECK (std::abs (gIn - 0.5f) < 1.0e-6f);
        PartitionedConvolver::computeCrossfadeGains (512, 1024, 0.0f, gOut, gIn);
        CHECK (std::abs (gOut - 0.70710678f) < 1.0e-6f);
        CHECK (std::abs (gIn - 0.70710678f) < 1.0e-6f);
        PartitionedConvolver::computeCrossfadeGains (1024, 1024, 0.5f, gOut, gIn);
        CHECK (gOut == 0.0f);
        CHECK (gIn == 1.0f);
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
