#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Binaural/BinauralRenderer.h>
#include "BinauralMetrics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;
using Catch::Matchers::WithinRel;

// ============================================================================
// BinauralRenderer — dedicated tests (EXTR-02).
//
// Each TEST_CASE builds its own renderer(s), so nothing is shared between cases except
// the lock-protected process-global SharedFFTCache: a ctest per-test-process run and one
// SpatialCoreTests run give the same verdict.
// ============================================================================

namespace
{
    constexpr double kRate = 48000.0;

    float rad (float deg) { return juce::degreesToRadians (deg); }

    // Renders `input` through source slot 0 of a bare BinauralRenderer in blocks of `blockSize`.
    // Slot 0's HRIR must already have been set with updateSourceHRIR().
    StereoSignal renderSlot0 (BinauralRenderer& renderer, const std::vector<float>& input, int blockSize)
    {
        StereoSignal out;
        out.left.assign (input.size(), 0.0f);
        out.right.assign (input.size(), 0.0f);

        bool enabled[MAX_SOURCES] = {};
        enabled[0] = true;
        std::vector<float> blockL (static_cast<size_t> (blockSize), 0.0f);
        std::vector<float> blockR (static_cast<size_t> (blockSize), 0.0f);

        for (size_t pos = 0; pos < input.size(); )
        {
            const size_t n = std::min (static_cast<size_t> (blockSize), input.size() - pos);
            const float* bufs[MAX_SOURCES];
            for (auto& b : bufs)
                b = input.data() + pos;

            renderer.renderSourceBuffers (bufs, enabled, MAX_SOURCES, static_cast<int> (n), blockL.data(), blockR.data());
            std::copy (blockL.begin(), blockL.begin() + static_cast<std::ptrdiff_t> (n), out.left.begin() + static_cast<std::ptrdiff_t> (pos));
            std::copy (blockR.begin(), blockR.begin() + static_cast<std::ptrdiff_t> (n), out.right.begin() + static_cast<std::ptrdiff_t> (pos));
            pos += n;
        }
        return out;
    }

    std::vector<float> impulseOfLength (size_t n)
    {
        std::vector<float> x (n, 0.0f);
        x[0] = 1.0f;
        return x;
    }

    bool allExactlyZero (const StereoSignal& s)
    {
        for (float v : s.left)
            if (v != 0.0f)
                return false;
        for (float v : s.right)
            if (v != 0.0f)
                return false;
        return true;
    }

    // First sample whose magnitude is at least 0.1 x the peak of x[0, count).
    int onsetOf (const float* x, int count)
    {
        float peak = 0.0f;
        for (int i = 0; i < count; ++i)
            peak = std::max (peak, std::abs (x[i]));
        for (int i = 0; i < count; ++i)
            if (std::abs (x[i]) >= 0.1f * peak)
                return i;
        return 0;
    }
}

TEST_CASE ("BinauralRenderer: profile 0 is Simple mode and renders exact silence",
           "[renderer][simple]")
{
    BinauralRenderer renderer;
    renderer.prepare (kRate, 512);
    renderer.setProfile (0);
    REQUIRE (renderer.isSimpleMode());
    CHECK (renderer.getActiveProfile() == 0);

    renderer.updateSourceHRIR (0, 0.5f, 0.0f);

    const std::vector<float> noise = whiteNoise (2048, 31, 0.5f);
    const StereoSignal out = renderSlot0 (renderer, noise, 512);
    CHECK (allExactlyZero (out));
}

TEST_CASE ("BinauralRenderer: setProfile scales the reference-direction energy to 1/sqrt(irLen)",
           "[renderer][normalisation]")
{
    struct Ref { float az, el; };
    constexpr float pi = juce::MathConstants<float>::pi;
    // The six directions BinauralRenderer::setProfile samples (BinauralRenderer.cpp refDirs).
    const Ref refDirs[] = { { 0.0f, 0.0f }, { pi, 0.0f }, { pi * 0.5f, 0.0f },
                            { -pi * 0.5f, 0.0f }, { 0.0f, pi * 0.25f }, { 0.0f, -pi * 0.25f } };

    for (int profile = 1; profile <= 4; ++profile)
    {
        BinauralRenderer renderer;
        renderer.prepare (kRate, 512);
        REQUIRE (loadProfileIntoRenderer (renderer, profile, kRate));
        renderer.setITDEnabled (false);   // HRIRs are then the interpolated ones setProfile measured

        const int irLen = renderer.hrtfDatabase.getIRLength();
        REQUIRE (irLen > 0);
        REQUIRE (irLen <= 512);

        double energy = 0.0;
        for (const auto& d : refDirs)
        {
            renderer.invalidateSources();
            renderer.updateSourceHRIR (0, d.az, d.el);
            const StereoSignal ir = renderSlot0 (renderer, impulseOfLength (512), 512);
            for (int n = 0; n < irLen; ++n)
            {
                energy += static_cast<double> (ir.left[static_cast<size_t> (n)]) * ir.left[static_cast<size_t> (n)];
                energy += static_cast<double> (ir.right[static_cast<size_t> (n)]) * ir.right[static_cast<size_t> (n)];
            }
        }

        const double rms = std::sqrt (energy / (6.0 * 2.0 * irLen));
        const double target = 1.0 / std::sqrt (static_cast<double> (irLen));
        INFO ("profile " << profile << " irLen " << irLen << " rms " << rms << " target " << target);
        CHECK_THAT (rms, WithinRel (target, 1.0e-4));
    }
}

TEST_CASE ("BinauralRenderer: the KEMAR low shelf is keyed on profile index 5 only",
           "[renderer][kemar-shelf]")
{
    // Same KEMAR data under three profile indices: only index 5 applies the +12 dB shelf (D-07).
    auto leftSum = [] (int profileIndex)
    {
        BinauralRenderer renderer;
        renderer.prepare (kRate, 512);
        REQUIRE (renderer.hrtfDatabase.loadFromFile (getSofaFile ("mit_kemar_large_pinna.sofa"),
                                                      static_cast<float> (kRate)));
        renderer.setProfile (profileIndex);
        renderer.setITDEnabled (false);
        renderer.updateSourceHRIR (0, rad (90.0f), 0.0f);

        const StereoSignal out = renderSlot0 (renderer, impulseOfLength (16384), 512);
        double sum = 0.0;
        for (float v : out.left)
            sum += static_cast<double> (v);
        return sum;
    };

    const double sum5 = leftSum (5);
    const double sum4 = leftSum (4);
    const double sum3 = leftSum (3);

    REQUIRE (std::abs (sum4) > 1.0e-3);
    const double expectedRatio = std::pow (10.0, 12.0 / 20.0);
    INFO ("DC sums: index5 " << sum5 << ", index4 " << sum4 << ", index3 " << sum3);
    CHECK_THAT (sum5 / sum4, WithinRel (expectedRatio, 0.02));
    CHECK_THAT (sum3, WithinRel (sum4, 1.0e-5));
}

TEST_CASE ("BinauralRenderer: reset() and invalidateSources() clear the ITD state and render silence from silence",
           "[renderer][reset]")
{
    for (const bool useReset : { true, false })
    {
        INFO ("path: " << (useReset ? "reset()" : "invalidateSources()"));

        BinauralRenderer renderer;
        renderer.prepare (kRate, 512);
        REQUIRE (loadProfileIntoRenderer (renderer, 5, kRate));   // KEMAR, ITD active

        renderer.updateSourceHRIR (0, rad (90.0f), 0.0f);
        const StereoSignal noisy = renderSlot0 (renderer, whiteNoise (4 * 512, 32, 0.5f), 512);
        REQUIRE (signalIsFinite (noisy));
        CHECK ((renderer.getTargetITDL (0) != 0.0f || renderer.getTargetITDR (0) != 0.0f));
        CHECK ((renderer.getCurrentITDL (0) != 0.0f || renderer.getCurrentITDR (0) != 0.0f));

        if (useReset)
            renderer.reset();
        else
            renderer.invalidateSources();

        CHECK (renderer.getCurrentITDL (0) == 0.0f);
        CHECK (renderer.getCurrentITDR (0) == 0.0f);
        CHECK (renderer.getTargetITDL (0) == 0.0f);
        CHECK (renderer.getTargetITDR (0) == 0.0f);

        renderer.updateSourceHRIR (0, rad (90.0f), 0.0f);   // re-arm
        const StereoSignal silent = renderSlot0 (renderer, std::vector<float> (4 * 512, 0.0f), 512);
        CHECK (allExactlyZero (silent));
    }
}

// ----------------------------------------------------------------------------
// D-16: characterise the 64-sample ITD line, do not change it.
//
// BinauralRenderer keeps a 64-sample ITD delay line (kITDBufferSize) and masks the read
// index with & 63, but HRTFDatabase::getAlignedHRIR returns the ABSOLUTE onset delay, which
// reaches 123 samples for SADIE at 48 kHz. The rendered delay is therefore
// (floor (delay) mod 64) plus the fraction, not the raw delay. That is the sound OpenSpatialDelay
// ships; Phase 3 pins it so any later change is a deliberate, visible decision.
// ----------------------------------------------------------------------------
namespace
{
    struct ItdCase
    {
        int profile;
        double rate;
        float azDeg, elDeg;
        float delayL, delayR;   // raw getTargetITDL/R (0)
        int onsetL, onsetR;     // rendered per-ear onset, samples
    };

    // captured from this tree on 2026-10-04 at commit 7453d93, libmysofa v1.3.2; D-16 pins today's 64-sample ITD line; a change here is a change to shipped timing and needs a decision
    constexpr ItdCase kItdTable[] = {
        { 1, 44100.0, 0.0f, 0.0f, 75.000f, 79.000f, 86, 90 },
        { 1, 44100.0, 90.0f, 0.0f, 71.000f, 93.000f, 78, 100 },
        { 1, 44100.0, -90.0f, 0.0f, 91.000f, 74.000f, 101, 84 },
        { 1, 44100.0, 180.0f, 0.0f, 75.000f, 76.000f, 86, 87 },
        { 1, 44100.0, 45.0f, 30.0f, 78.000f, 83.000f, 92, 97 },
        { 1, 48000.0, 0.0f, 0.0f, 82.000f, 86.000f, 100, 104 },
        { 1, 48000.0, 90.0f, 0.0f, 72.000f, 102.000f, 80, 110 },
        { 1, 48000.0, -90.0f, 0.0f, 66.000f, 82.000f, 68, 84 },
        { 1, 48000.0, 180.0f, 0.0f, 86.000f, 87.000f, 108, 109 },
        { 1, 48000.0, 45.0f, 30.0f, 83.000f, 91.000f, 102, 110 },
        { 5, 44100.0, 0.0f, 0.0f, 38.000f, 38.000f, 76, 76 },
        { 5, 44100.0, 90.0f, 0.0f, 29.000f, 56.000f, 58, 85 },
        { 5, 44100.0, -90.0f, 0.0f, 56.000f, 29.000f, 85, 58 },
        { 5, 44100.0, 180.0f, 0.0f, 40.000f, 40.000f, 80, 80 },
        { 5, 44100.0, 45.0f, 30.0f, 31.000f, 45.000f, 62, 76 },
        { 5, 48000.0, 0.0f, 0.0f, 41.000f, 41.000f, 82, 82 },
        { 5, 48000.0, 90.0f, 0.0f, 31.000f, 61.000f, 62, 92 },
        { 5, 48000.0, -90.0f, 0.0f, 61.000f, 31.000f, 92, 62 },
        { 5, 48000.0, 180.0f, 0.0f, 43.000f, 43.000f, 86, 86 },
        { 5, 48000.0, 45.0f, 30.0f, 34.000f, 49.000f, 68, 83 },
    };

    constexpr float kItdDirections[5][2] = { { 0.0f, 0.0f }, { 90.0f, 0.0f }, { -90.0f, 0.0f }, { 180.0f, 0.0f }, { 45.0f, 30.0f } };
}

TEST_CASE ("BinauralRenderer: the 64-sample ITD line is characterised at 44.1 and 48 kHz (D-16)",
           "[renderer][itd-characterisation]")
{
    constexpr int kCapture = 512;
    const int profiles[] = { 1, 5 };
    const double rates[] = { 44100.0, 48000.0 };

    size_t row = 0;
    float maxSadie48Delay = 0.0f;

    for (int profile : profiles)
    {
        for (double rate : rates)
        {
            for (const auto& dir : kItdDirections)
            {
                BinauralRenderer renderer;
                renderer.prepare (rate, 512);
                REQUIRE (loadProfileIntoRenderer (renderer, profile, rate));

                const float az = rad (dir[0]);
                const float el = rad (dir[1]);
                renderer.updateSourceHRIR (0, az, el);

                // One silent block lets currentITD reach the target; the impulse then sees a constant delay.
                std::vector<float> input (2 * 512, 0.0f);
                input[512] = 1.0f;
                const StereoSignal rendered = renderSlot0 (renderer, input, 512);

                const float delayL = renderer.getTargetITDL (0);
                const float delayR = renderer.getTargetITDR (0);
                const int onsetL = onsetOf (rendered.left.data() + 512, kCapture);
                const int onsetR = onsetOf (rendered.right.data() + 512, kCapture);

                std::vector<float> alignedL (static_cast<size_t> (renderer.hrtfDatabase.getIRLength()));
                std::vector<float> alignedR (alignedL.size());
                float dL = 0.0f, dR = 0.0f;
                renderer.hrtfDatabase.getAlignedHRIR (az, el, alignedL.data(), alignedR.data(), dL, dR);
                const int alignedOnsetL = onsetOf (alignedL.data(), static_cast<int> (alignedL.size()));
                const int alignedOnsetR = onsetOf (alignedR.data(), static_cast<int> (alignedR.size()));

                char line[256];
                std::snprintf (line, sizeof (line), "{ %d, %.1f, %.1ff, %.1ff, %.3ff, %.3ff, %d, %d },",
                               profile, rate, dir[0], dir[1], delayL, delayR, onsetL, onsetR);
                WARN (line);

                const int wrapL = static_cast<int> (std::floor (delayL)) % 64;
                const int wrapR = static_cast<int> (std::floor (delayR)) % 64;
                INFO ("profile " << profile << " rate " << rate << " az " << dir[0] << " el " << dir[1]);
                CHECK (std::abs (onsetL - (alignedOnsetL + wrapL)) <= 1);
                CHECK (std::abs (onsetR - (alignedOnsetR + wrapR)) <= 1);

                if (row < std::size (kItdTable))
                {
                    const ItdCase& pinned = kItdTable[row];
                    CHECK (pinned.profile == profile);
                    CHECK (pinned.rate == rate);
                    CHECK (pinned.azDeg == dir[0]);
                    CHECK (pinned.elDeg == dir[1]);
                    CHECK (std::abs (delayL - pinned.delayL) <= 1.0e-3f);
                    CHECK (std::abs (delayR - pinned.delayR) <= 1.0e-3f);
                    CHECK (onsetL == pinned.onsetL);
                    CHECK (onsetR == pinned.onsetR);
                }
                ++row;

                if (profile == 1 && rate == 48000.0)
                    maxSadie48Delay = std::max ({ maxSadie48Delay, delayL, delayR });
            }
        }
    }

    CHECK (row == std::size (kItdTable));
    INFO ("largest SADIE 48 kHz raw delay " << maxSadie48Delay << " samples");
    CHECK (maxSadie48Delay >= 64.0f);
}

// ============================================================================
// BUG-02 follow-on (03-RESEARCH.md Pitfall 4): KEMAR's HRIR is 558 samples, longer than the
// 512-sample scratch the renderer used to size in prepare(). updateSourceHRIR() then grew the
// scratch buffers on the audio thread (a jassertfalse in Debug). The buffers are now sized
// for the loaded IR in prepare() and setProfile(), which run on the message or loader thread.
//
// The allocation itself is not observable from here, so besides the finite, non-silent output
// this test is judged by the Debug log: a run must print no
// "JUCE Assertion failure in BinauralRenderer.cpp" line (the 03-03 plan's Task 3 verify
// counts them). The renderSourceBuffers guard and the jassertfalse pattern stay as they are;
// they are RTSF-01 (Phase 5).
// ============================================================================

namespace
{
    // Renders 20 blocks of `blockSize` with source 0 sweeping 12 degrees per block (well above
    // the renderer's 1 degree HRIR-update threshold, so every block loads a new HRIR) and
    // returns the whole output. The renderer's profile must already be loaded.
    StereoSignal renderMovingSource (BinauralRenderer& renderer, int blockSize)
    {
        constexpr int kBlocks = 20;
        const std::vector<float> input = whiteNoise (static_cast<size_t> (kBlocks * blockSize), 41, 0.25f);

        StereoSignal out;
        bool enabled[MAX_SOURCES] = {};
        enabled[0] = true;
        std::vector<float> blockL (static_cast<size_t> (blockSize), 0.0f);
        std::vector<float> blockR (static_cast<size_t> (blockSize), 0.0f);

        for (int b = 0; b < kBlocks; ++b)
        {
            renderer.updateSourceHRIR (0, rad (-60.0f + 12.0f * static_cast<float> (b)), 0.0f);

            const float* bufs[MAX_SOURCES];
            for (auto& p : bufs)
                p = input.data() + static_cast<size_t> (b * blockSize);

            renderer.renderSourceBuffers (bufs, enabled, MAX_SOURCES, blockSize, blockL.data(), blockR.data());
            out.left.insert (out.left.end(), blockL.begin(), blockL.end());
            out.right.insert (out.right.end(), blockR.begin(), blockR.end());
        }
        return out;
    }

    float peakMagnitude (const StereoSignal& s)
    {
        float peak = 0.0f;
        for (float v : s.left)
            peak = std::max (peak, std::abs (v));
        for (float v : s.right)
            peak = std::max (peak, std::abs (v));
        return peak;
    }
}

TEST_CASE ("BinauralRenderer: KEMAR's 558-sample IR is prepared off the audio thread at every block size",
           "[renderer][scratch]")
{
    SECTION ("loaded after prepare() at maximum block 64, 256 and 512")
    {
        for (int maxBlock : { 64, 256, 512 })
        {
            BinauralRenderer renderer;
            renderer.prepare (kRate, maxBlock);
            REQUIRE (loadProfileIntoRenderer (renderer, 5, kRate));
            REQUIRE (renderer.hrtfDatabase.getIRLength() == 558);

            const StereoSignal out = renderMovingSource (renderer, maxBlock);

            INFO ("maximum block " << maxBlock);
            CHECK (signalIsFinite (out));
            CHECK (peakMagnitude (out) > 1.0e-3f);
        }
    }

    SECTION ("prepare() again at a smaller block size keeps room for the loaded IR")
    {
        BinauralRenderer renderer;
        renderer.prepare (kRate, 512);
        REQUIRE (loadProfileIntoRenderer (renderer, 5, kRate));

        renderer.prepare (kRate, 64);

        const StereoSignal out = renderMovingSource (renderer, 64);
        CHECK (signalIsFinite (out));
        CHECK (peakMagnitude (out) > 1.0e-3f);
    }
}
