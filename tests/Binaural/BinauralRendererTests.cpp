#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Binaural/BinauralRenderer.h>
#include "BinauralMetrics.h"
#include "../Support/RealtimeGuard.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
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
// The ITD line, characterised at 44.1 and 48 kHz (D-16, superseded by
// Spatial-Media-Lab/OpenSpatialDelay#234 D-13 and WR-01, AndrewRahman/SpatialCore#25).
//
// Phase 3 pinned the 64-sample line, which wrapped the ABSOLUTE onset delays (SADIE up to
// 123 samples at 48 kHz) modulo 64. the Spatial-Media-Lab/OpenSpatialDelay#234 work, decided and A/B-tested there (O211,
// O212), fixed both halves of SpatialCore#25's intended fix: the ring is kITDBufferSize = 512
// with delays clamped to kMaxITDSamples (D-13), and getAlignedHRIR reports each ear's onset
// SHIFT (onset - minOnset: 0 for the leading ear, the ITD for the other) instead of the
// absolute onset (WR-01), so minOnset is no longer applied twice. The table pins the new
// timing: the delays are the shifts, and the rendered per-ear onset equals the raw HRIR onset.
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

    // captured 2026-10-24 on SpatialCore osd-234-sadie-onset (O214 detectOnsetEnergy onset; SADIE +-90 az now symmetric 30/29 instead of 30/16; rendered onsets unchanged) over osd-234-on-main (the Spatial-Media-Lab/OpenSpatialDelay#234 512-sample ring + WR-01 onset shift), libmysofa v1.3.5; a change here is a change to shipped timing and needs a decision
    constexpr ItdCase kItdTable[] = {
    { 1, 44100.0, 0.0f, 0.0f, 0.000f, 0.000f, 75, 79 },
    { 1, 44100.0, 90.0f, 0.0f, 0.000f, 27.000f, 71, 93 },
    { 1, 44100.0, -90.0f, 0.0f, 27.000f, 0.000f, 91, 74 },
    { 1, 44100.0, 180.0f, 0.0f, 0.000f, 1.000f, 75, 76 },
    { 1, 44100.0, 45.0f, 30.0f, 0.000f, 10.000f, 78, 83 },
    { 1, 48000.0, 0.0f, 0.0f, 1.000f, 0.000f, 82, 86 },
    { 1, 48000.0, 90.0f, 0.0f, 0.000f, 30.000f, 72, 102 },
    { 1, 48000.0, -90.0f, 0.0f, 29.000f, 0.000f, 66, 82 },
    { 1, 48000.0, 180.0f, 0.0f, 0.000f, 1.000f, 86, 87 },
    { 1, 48000.0, 45.0f, 30.0f, 0.000f, 12.000f, 83, 91 },
    { 5, 44100.0, 0.0f, 0.0f, 0.000f, 0.000f, 38, 38 },
    { 5, 44100.0, 90.0f, 0.0f, 0.000f, 31.000f, 29, 56 },
    { 5, 44100.0, -90.0f, 0.0f, 31.000f, 0.000f, 56, 29 },
    { 5, 44100.0, 180.0f, 0.0f, 0.000f, 0.000f, 40, 40 },
    { 5, 44100.0, 45.0f, 30.0f, 0.000f, 14.000f, 31, 45 },
    { 5, 48000.0, 0.0f, 0.0f, 0.000f, 0.000f, 41, 41 },
    { 5, 48000.0, 90.0f, 0.0f, 0.000f, 33.000f, 31, 61 },
    { 5, 48000.0, -90.0f, 0.0f, 33.000f, 0.000f, 61, 31 },
    { 5, 48000.0, 180.0f, 0.0f, 0.000f, 0.000f, 43, 43 },
    { 5, 48000.0, 45.0f, 30.0f, 0.000f, 16.000f, 34, 49 },
    };

    constexpr float kItdDirections[5][2] = { { 0.0f, 0.0f }, { 90.0f, 0.0f }, { -90.0f, 0.0f }, { 180.0f, 0.0f }, { 45.0f, 30.0f } };
}

TEST_CASE ("BinauralRenderer: the 512-sample ITD line carries the onset shift at 44.1 and 48 kHz (D-16, D-13, WR-01)",
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

                std::vector<float> rawL (alignedL.size()), rawR (alignedL.size());
                float rawDL = 0.0f, rawDR = 0.0f;
                renderer.hrtfDatabase.getInterpolatedHRIR (az, el, rawL.data(), rawR.data(), rawDL, rawDR);
                const int rawOnsetL = onsetOf (rawL.data(), static_cast<int> (rawL.size()));
                const int rawOnsetR = onsetOf (rawR.data(), static_cast<int> (rawR.size()));

                char line[256];
                std::snprintf (line, sizeof (line), "{ %d, %.1f, %.1ff, %.1ff, %.3ff, %.3ff, %d, %d },",
                               profile, rate, dir[0], dir[1], delayL, delayR, onsetL, onsetR);
                WARN (line);

                INFO ("profile " << profile << " rate " << rate << " az " << dir[0] << " el " << dir[1]);
                // No wrap: the rendered onset is the aligned onset plus the whole delay ...
                CHECK (std::abs (onsetL - (alignedOnsetL + static_cast<int> (std::floor (delayL)))) <= 1);
                CHECK (std::abs (onsetR - (alignedOnsetR + static_cast<int> (std::floor (delayR)))) <= 1);
                // ... the delays are the shift (the leading ear gets none) ...
                CHECK (std::min (delayL, delayR) == 0.0f);
                CHECK (std::max (delayL, delayR) <= 510.0f);   // BinauralRenderer::kMaxITDSamples
                // ... and the rendered onset is the raw HRIR onset, not minOnset twice (WR-01).
                CHECK (std::abs (onsetL - rawOnsetL) <= 1);
                CHECK (std::abs (onsetR - rawOnsetR) <= 1);

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
    // SADIE's ITD shift stays well inside the ring (it was the absolute onset, up to 123, before).
    INFO ("largest SADIE 48 kHz delay " << maxSadie48Delay << " samples");
    CHECK (maxSadie48Delay > 0.0f);
    CHECK (maxSadie48Delay < 64.0f);
}

// ============================================================================
// BUG-02 follow-on (03-RESEARCH.md Pitfall 4): KEMAR's HRIR is 558 samples, longer than the
// 512-sample scratch the renderer used to size in prepare(). updateSourceHRIR() then grew the
// scratch buffers on the audio thread (a jassertfalse in Debug). The buffers are now sized
// for the loaded IR in prepare() and setProfile(), which run on the message or loader thread.
//
// The finite, non-silent output is checked here, and a run must print no
// "JUCE Assertion failure in BinauralRenderer.cpp" line (the 03-03 plan's Task 3 verify
// counts them). The two guards that used to grow the scratch on the audio thread no longer
// can: since Phase 5 (RTSF-01) they skip, count and never allocate, and the [rt][scratch]
// cases at the end of this file prove it.
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

// ============================================================================
// RTSF-01 (Phase 5, D-03, D-04): the two scratch-too-small guards never allocate.
//
// A scratch smaller than needed is reachable only when a caller renders more than prepare() was
// given, or (here) through shrinkScratchForTesting(). The guard then skips just that work, keeps
// the last good sound and counts the event; Debug builds also assert. Each case compares the
// renderer that took the hit with a twin built identically that never saw the bad call, bit for
// bit, so "nothing moved" is proven rather than assumed.
//
// A Debug assert allocates (JUCE logs through a juce::String), and in a Debug tree libmysofa
// printf()s on every HRIR lookup, so the allocation and lock checks run in Release only; the
// counter and the output checks run in every build. skipUnlessRealtimeTrap() sits in the Release
// branch after those checks, so an unavailable trap skips instead of reading as a false zero.
// ============================================================================

namespace
{
    constexpr int kScratchBlock = 64;
    constexpr int kScratchBlocks = 20;

    // KEMAR (558-sample IR at 48 kHz) loaded, source 0 pointed at az 30. The scratch is pinned to
    // exactly 558 floats, capacity included: vector growth in setProfile() leaves spare capacity,
    // which would hide an audio-thread resize from the trap.
    std::unique_ptr<BinauralRenderer> makeKemarRenderer()
    {
        auto renderer = std::make_unique<BinauralRenderer>();
        renderer->prepare (kRate, kScratchBlock);
        REQUIRE (loadProfileIntoRenderer (*renderer, 5, kRate));
        REQUIRE (renderer->hrtfDatabase.getIRLength() == 558);
        renderer->shrinkScratchForTesting (558);
        renderer->updateSourceHRIR (0, rad (30.0f), 0.0f);
        return renderer;
    }

    bool sameBits (const StereoSignal& a, const StereoSignal& b)
    {
        return a.left.size() == b.left.size() && a.right.size() == b.right.size()
            && std::memcmp (a.left.data(), b.left.data(), sizeof (float) * a.left.size()) == 0
            && std::memcmp (a.right.data(), b.right.data(), sizeof (float) * a.right.size()) == 0;
    }

    // Twenty normal blocks through source 0, the same noise every time it is called.
    StereoSignal renderNormalBlocks (BinauralRenderer& renderer)
    {
        const std::vector<float> input = whiteNoise (static_cast<size_t> (kScratchBlocks * kScratchBlock), 77, 0.25f);
        return renderSlot0 (renderer, input, kScratchBlock);
    }

    // Calls `call` with, in a Release tree, the trap armed around it only. The counts are those of
    // that call alone (all zero in a Debug tree, where they are not checked).
    template <typename Fn>
    RealtimeCounts trapped (Fn&& call)
    {
        RealtimeCounts counts;
#if ! JUCE_DEBUG
        resetRealtimeCounts();
        {
            const ScopedRealtimeTrap trap;
            call();
        }
        counts = realtimeCounts();
#else
        call();
#endif
        return counts;
    }

    void checkNothingAllocated ([[maybe_unused]] const RealtimeCounts& counts)
    {
#if ! JUCE_DEBUG
        skipUnlessRealtimeTrap();
        CAPTURE (counts.allocs, counts.frees, counts.locks);
        CHECK (counts.allocs == 0);
        CHECK (counts.frees == 0);
        CHECK (counts.locks == 0);
#endif
    }
}

TEST_CASE ("BinauralRenderer: renderSourceBuffers with a scratch that is too small skips, counts and keeps the last good sound",
           "[renderer][rt][scratch]")
{
    auto hit = makeKemarRenderer();
    auto twin = makeKemarRenderer();

    // 600 samples against a 558-float scratch.
    constexpr int kTooLong = 600;
    const std::vector<float> input = whiteNoise (static_cast<size_t> (kTooLong), 5, 0.25f);
    std::vector<float> outL (static_cast<size_t> (kTooLong), 7.0f), outR (static_cast<size_t> (kTooLong), 7.0f);
    bool enabled[MAX_SOURCES] = {};
    enabled[0] = true;
    const float* bufs[MAX_SOURCES];
    for (auto& b : bufs)
        b = input.data();

    CHECK (hit->getScratchTooSmallCount() == 0);

    const RealtimeCounts counts = trapped ([&]
    {
        hit->renderSourceBuffers (bufs, enabled, MAX_SOURCES, kTooLong, outL.data(), outR.data());
    });

    // Skipped: the zeroed output, no convolver advanced, counted once.
    CHECK (hit->getScratchTooSmallCount() == 1);
    CHECK (std::all_of (outL.begin(), outL.end(), [] (float v) { return v == 0.0f; }));
    CHECK (std::all_of (outR.begin(), outR.end(), [] (float v) { return v == 0.0f; }));

    // Keep the last good sound: the next normal blocks equal a renderer that never saw the call.
    const StereoSignal fromHit = renderNormalBlocks (*hit);
    const StereoSignal fromTwin = renderNormalBlocks (*twin);
    CHECK (peakMagnitude (fromTwin) > 1.0e-3f);
    CHECK (sameBits (fromHit, fromTwin));
    CHECK (hit->getScratchTooSmallCount() == 1);
    CHECK (twin->getScratchTooSmallCount() == 0);

    checkNothingAllocated (counts);
}

TEST_CASE ("BinauralRenderer: updateSourceHRIR with a scratch that is too small skips, counts and keeps the previous HRIR",
           "[renderer][rt][scratch]")
{
    auto hit = makeKemarRenderer();
    auto twin = makeKemarRenderer();

    // 100 floats is still enough for a 64-sample block, so rendering is unaffected.
    hit->shrinkScratchForTesting (100);

    const RealtimeCounts counts = trapped ([&] { hit->updateSourceHRIR (0, rad (-60.0f), 0.0f); });

    CHECK (hit->getScratchTooSmallCount() == 1);

    // The source kept the az 30 HRIR the twin still holds.
    const StereoSignal fromHit = renderNormalBlocks (*hit);
    const StereoSignal fromTwin = renderNormalBlocks (*twin);
    CHECK (peakMagnitude (fromTwin) > 1.0e-3f);
    CHECK (sameBits (fromHit, fromTwin));
    CHECK (hit->getScratchTooSmallCount() == 1);
    CHECK (twin->getScratchTooSmallCount() == 0);

    checkNothingAllocated (counts);
}

TEST_CASE ("BinauralRenderer: the scratch guards count nothing at the boundary and exactly once one sample short",
           "[renderer][rt][scratch]")
{
    SECTION ("updateSourceHRIR: a scratch exactly as long as the IR fits, one sample short is skipped")
    {
        auto moved = makeKemarRenderer();
        auto twin = makeKemarRenderer();

        moved->shrinkScratchForTesting (557);
        moved->updateSourceHRIR (0, rad (-60.0f), 0.0f);
        CHECK (moved->getScratchTooSmallCount() == 1);

        moved->shrinkScratchForTesting (558);
        moved->updateSourceHRIR (0, rad (-60.0f), 0.0f);   // fits now: loads the new HRIR
        CHECK (moved->getScratchTooSmallCount() == 1);

        // The new direction took effect, so the sound differs from the twin that stayed at az 30.
        const StereoSignal fromMoved = renderNormalBlocks (*moved);
        const StereoSignal fromTwin = renderNormalBlocks (*twin);
        CHECK (signalIsFinite (fromMoved));
        CHECK (! sameBits (fromMoved, fromTwin));
    }

    SECTION ("renderSourceBuffers: exactly the scratch length fits, one sample more is skipped, empty counts nothing")
    {
        auto renderer = makeKemarRenderer();
        constexpr int kFits = 558;
        const std::vector<float> input = whiteNoise (static_cast<size_t> (kFits + 1), 9, 0.25f);
        std::vector<float> outL (static_cast<size_t> (kFits + 1), 0.0f), outR (static_cast<size_t> (kFits + 1), 0.0f);
        bool enabled[MAX_SOURCES] = {};
        enabled[0] = true;
        const float* bufs[MAX_SOURCES];
        for (auto& b : bufs)
            b = input.data();

        renderer->renderSourceBuffers (bufs, enabled, MAX_SOURCES, kFits, outL.data(), outR.data());
        CHECK (renderer->getScratchTooSmallCount() == 0);
        CHECK (signalIsFinite ({ outL, outR }));

        renderer->renderSourceBuffers (bufs, enabled, MAX_SOURCES, 0, outL.data(), outR.data());
        CHECK (renderer->getScratchTooSmallCount() == 0);

        renderer->renderSourceBuffers (bufs, enabled, MAX_SOURCES, kFits + 1, outL.data(), outR.data());
        CHECK (renderer->getScratchTooSmallCount() == 1);
    }
}
