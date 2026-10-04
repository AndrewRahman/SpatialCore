#pragma once

// Shared Phase 3 measurement header (binaural defects and HRTF packaging).
//
// Every helper the Phase 3 tests need lives here, written once so that later plans
// include it and never edit it: an engine driver, an engine impulse response, the
// third-octave distinguishability metric (BUG-01), ITU-R BS.1770 K-weighting (D-14),
// profile-switch continuity metrics (DATA-01), seeded noise and sine generators, a
// double-precision direct-convolution oracle (BUG-02, D-12), the block plans every
// block-size test uses, and a synchronous profile loader.
//
// LANDMINE (same as BinauralTestUtilities.h): every SOFA-profile test must load the
// profile synchronously before it renders. loadProfileIntoRenderer() and
// loadProfileIntoActiveRenderer() do that and are the only way these tests load one.

#include "BinauralTestUtilities.h"
#include <SpatialCore/Engine/RenderEngine.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace spatialcore::test
{

//==============================================================================
// Profile loading (D-07 numbering: 0 = Simple Woodworth, 1-5 = built-in SOFA files)
//==============================================================================

/** D-07 filename of built-in profile 1..5, nullptr for any other index. */
inline const char* testProfileFile (int profile)
{
    switch (profile)
    {
        case 1: return "sadie_d2_ku100.sofa";
        case 2: return "cipic_subject_003.sofa";
        case 3: return "hutubs_pp2.sofa";
        case 4: return "bernschuetz_ku100.sofa";
        case 5: return "mit_kemar_large_pinna.sofa";
        default: return nullptr;
    }
}

/** Synchronously loads a profile into a renderer and activates it. Profile 0 unloads the
    database and selects Simple mode. Returns the SOFA load result (true for profile 0).
    The renderer must already have been prepare()d. */
inline bool loadProfileIntoRenderer (BinauralRenderer& renderer, int profile, double sampleRate)
{
    if (const char* file = testProfileFile (profile))
    {
        const bool ok = renderer.hrtfDatabase.loadFromFile (getSofaFile (file), static_cast<float> (sampleRate));
        renderer.setProfile (profile);
        return ok;
    }

    renderer.hrtfDatabase.unload();
    renderer.setProfile (0);
    return true;
}

/** Same as loadProfileIntoRenderer, on the engine's currently active renderer. */
inline bool loadProfileIntoActiveRenderer (RenderEngine& engine, int profile, double sampleRate)
{
    auto& renderer = engine.getBinauralRenderer (engine.getActiveRendererIndexAtomic().load());
    return loadProfileIntoRenderer (renderer, profile, sampleRate);
}

//==============================================================================
// Signal and direction types
//==============================================================================

/** Two-channel float signal, one vector per ear. */
struct StereoSignal
{
    std::vector<float> left, right;
};

/** Source direction in degrees (our convention: az 0 front, +90 left; el +90 overhead). */
struct Direction
{
    float azDeg;
    float elDeg;
};

/** Which binaural render path the engine dispatches to. */
enum class BinauralPath { Simple, HRTF };

/** True when every sample of both channels is finite. */
inline bool signalIsFinite (const StereoSignal& s)
{
    for (float v : s.left)
        if (! std::isfinite (v))
            return false;
    for (float v : s.right)
        if (! std::isfinite (v))
            return false;
    return true;
}

//==============================================================================
// Engine driver
//==============================================================================

/** Block context for a binaural render: engine-computed gains, caller-chosen path. */
inline RenderBlockContext makeBinauralContext (BinauralPath path, double sampleRate)
{
    RenderBlockContext ctx;
    ctx.isBinaural = true;
    ctx.useHRTF = (path == BinauralPath::HRTF);
    ctx.engineComputesGains = true;
    ctx.sampleRate = sampleRate;
    return ctx;
}

/** Renders `input` through the engine with one live source at slot 0. Cycles through the
    sizes in `blockPlan` (the last block is clamped to what remains), takes the source
    direction from `direction (blockStart)`, calls `beforeBlock (blockStart)` first when set,
    and returns a signal as long as the input. The engine must have been prepare()d with a
    maximum block size at least as large as the largest plan entry. */
inline StereoSignal renderThroughEngine (RenderEngine& engine,
                                          const RenderBlockContext& ctx,
                                          const std::vector<float>& input,
                                          const std::vector<int>& blockPlan,
                                          const std::function<Direction (int64_t blockStart)>& direction,
                                          const std::function<void (int64_t blockStart)>& beforeBlock = {},
                                          float distance = 0.5f)
{
    StereoSignal out;
    out.left.assign (input.size(), 0.0f);
    out.right.assign (input.size(), 0.0f);

    const int maxBlock = *std::max_element (blockPlan.begin(), blockPlan.end());
    std::vector<float> tapFade (static_cast<size_t> (maxBlock), 1.0f);
    std::vector<float> distGain (static_cast<size_t> (maxBlock), 1.0f);
    std::vector<float> blockL (static_cast<size_t> (maxBlock), 0.0f);
    std::vector<float> blockR (static_cast<size_t> (maxBlock), 0.0f);
    float* outPtrs[2] = { blockL.data(), blockR.data() };

    size_t pos = 0;
    size_t planIndex = 0;
    while (pos < input.size())
    {
        const size_t planned = static_cast<size_t> (blockPlan[planIndex % blockPlan.size()]);
        const size_t n = std::min (planned, input.size() - pos);
        ++planIndex;

        if (beforeBlock)
            beforeBlock (static_cast<int64_t> (pos));

        const Direction d = direction (static_cast<int64_t> (pos));

        RenderSources sources;
        sources.numSamples = static_cast<int> (n);
        sources.monoBuffers[0] = input.data() + pos;
        sources.tapFadeGainPerSample[0] = tapFade.data();
        sources.distGainPerSample[0] = distGain.data();
        sources.objectLive[0] = true;
        sources.objects[0].azimuthDeg = d.azDeg;
        sources.objects[0].elevationDeg = d.elDeg;
        sources.objects[0].distance = distance;
        sources.objects[0].enabled = true;

        engine.renderBlock (sources, ctx, outPtrs, 2);

        std::copy (blockL.begin(), blockL.begin() + static_cast<std::ptrdiff_t> (n), out.left.begin() + static_cast<std::ptrdiff_t> (pos));
        std::copy (blockR.begin(), blockR.begin() + static_cast<std::ptrdiff_t> (n), out.right.begin() + static_cast<std::ptrdiff_t> (pos));
        pos += n;
    }

    return out;
}

/** Impulse response of the engine at a fixed direction. Renders `settleBlocks` blocks of 512
    silent samples first (so gain ramps, HRIR loads and crossfades finish and the chain is
    time-invariant), then one unit impulse at the start of the next block, and returns
    `irLength` samples per ear starting at that impulse. The engine must be prepared with a
    maximum block size of at least 512 and have its profile loaded. */
inline StereoSignal engineImpulseResponse (RenderEngine& engine, BinauralPath path, Direction dir,
                                            double sampleRate, int irLength = 4096, int settleBlocks = 12)
{
    const size_t settleSamples = static_cast<size_t> (settleBlocks) * 512u;
    std::vector<float> input (settleSamples + static_cast<size_t> (irLength), 0.0f);
    input[settleSamples] = 1.0f;

    const RenderBlockContext ctx = makeBinauralContext (path, sampleRate);
    const StereoSignal rendered = renderThroughEngine (engine, ctx, input, { 512 },
                                                        [dir] (int64_t) { return dir; });

    StereoSignal ir;
    ir.left.assign (rendered.left.begin() + static_cast<std::ptrdiff_t> (settleSamples), rendered.left.end());
    ir.right.assign (rendered.right.begin() + static_cast<std::ptrdiff_t> (settleSamples), rendered.right.end());
    return ir;
}

//==============================================================================
// Third-octave distinguishability metric (BUG-01, D-03)
//==============================================================================

/** The 20 third-octave band centres, 200 Hz to 16 kHz. */
inline constexpr std::array<double, 20> kThirdOctaveCentresHz {
    200.0, 250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0,
    2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0
};

/** Mean spectral power per third-octave band of an impulse response: 4096-point real FFT of
    a zero-padded or truncated copy, band edges at centre * 2^(+-1/6), mean bin power inside
    each band. */
inline std::array<double, 20> thirdOctaveBandPowers (const std::vector<float>& ir, double sampleRate)
{
    constexpr int kOrder = 12;
    constexpr int kSize = 1 << kOrder;

    juce::dsp::FFT fft (kOrder);
    std::vector<float> work (static_cast<size_t> (kSize) * 2u, 0.0f);
    const size_t copyCount = std::min (ir.size(), static_cast<size_t> (kSize));
    std::copy (ir.begin(), ir.begin() + static_cast<std::ptrdiff_t> (copyCount), work.begin());
    fft.performRealOnlyForwardTransform (work.data(), true);

    std::array<double, 20> powers {};
    const double binHz = sampleRate / static_cast<double> (kSize);
    const double edge = std::pow (2.0, 1.0 / 6.0);

    for (size_t b = 0; b < powers.size(); ++b)
    {
        const double lo = kThirdOctaveCentresHz[b] / edge;
        const double hi = kThirdOctaveCentresHz[b] * edge;
        double sum = 0.0;
        int count = 0;

        for (int k = 1; k < kSize / 2; ++k)
        {
            const double f = static_cast<double> (k) * binHz;
            if (f < lo || f >= hi)
                continue;
            const double re = work[static_cast<size_t> (2 * k)];
            const double im = work[static_cast<size_t> (2 * k + 1)];
            sum += re * re + im * im;
            ++count;
        }

        powers[b] = (count > 0) ? sum / static_cast<double> (count) : 0.0;
    }

    return powers;
}

/** RMS and largest absolute third-octave band difference between two responses, in dB. */
struct BandDifference
{
    double rmsDb;
    double maxDb;
};

/** Per band, averages the L and R powers, takes the dB difference of `a` against `b`, and
    returns the RMS over the 20 bands and the largest absolute band difference. */
inline BandDifference bandDifference (const StereoSignal& a, const StereoSignal& b, double sampleRate)
{
    const auto aL = thirdOctaveBandPowers (a.left, sampleRate);
    const auto aR = thirdOctaveBandPowers (a.right, sampleRate);
    const auto bL = thirdOctaveBandPowers (b.left, sampleRate);
    const auto bR = thirdOctaveBandPowers (b.right, sampleRate);

    constexpr double floorPower = 1.0e-30;
    double sumSq = 0.0;
    double maxAbs = 0.0;

    for (size_t i = 0; i < aL.size(); ++i)
    {
        const double pa = std::max (0.5 * (aL[i] + aR[i]), floorPower);
        const double pb = std::max (0.5 * (bL[i] + bR[i]), floorPower);
        const double dB = 10.0 * std::log10 (pa / pb);
        sumSq += dB * dB;
        maxAbs = std::max (maxAbs, std::abs (dB));
    }

    return { std::sqrt (sumSq / static_cast<double> (aL.size())), maxAbs };
}

//==============================================================================
// K-weighted loudness (ITU-R BS.1770-4, D-14)
//==============================================================================

/** ITU-R BS.1770-4 K-weighted loudness of a stereo signal in LKFS, double precision, using
    the 48 kHz coefficients (high-shelf stage then the RLB high-pass). Channel weights are
    1.0 for L and R. Returns NaN for any other sample rate so misuse fails a finiteness check. */
inline double kWeightedLoudnessLKFS (const StereoSignal& s, double sampleRate)
{
    if (sampleRate != 48000.0 || s.left.empty() || s.left.size() != s.right.size())
        return std::numeric_limits<double>::quiet_NaN();

    struct Biquad
    {
        double b0, b1, b2, a1, a2;
        double z1 = 0.0, z2 = 0.0;

        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    auto meanSquareK = [] (const std::vector<float>& x)
    {
        Biquad shelf { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        Biquad rlb { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        double sum = 0.0;
        for (float v : x)
        {
            const double y = rlb.process (shelf.process (static_cast<double> (v)));
            sum += y * y;
        }
        return sum / static_cast<double> (x.size());
    };

    const double power = meanSquareK (s.left) + meanSquareK (s.right);
    return -0.691 + 10.0 * std::log10 (std::max (power, 1.0e-30));
}

//==============================================================================
// Deterministic generators
//==============================================================================

/** Seeded xorshift32 noise, uniform in [-amplitude, amplitude]. */
inline std::vector<float> whiteNoise (size_t n, uint32_t seed, float amplitude)
{
    std::vector<float> x (n);
    uint32_t state = (seed != 0u) ? seed : 1u;
    for (auto& v : x)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        v = amplitude * static_cast<float> (static_cast<double> (state) / 2147483648.0 - 1.0);
    }
    return x;
}

/** Seeded pink noise: Paul Kellet's refined filter over whiteNoise, scaled so the peak is
    at most `amplitude`. */
inline std::vector<float> pinkNoise (size_t n, uint32_t seed, float amplitude)
{
    const std::vector<float> white = whiteNoise (n, seed, 1.0f);
    std::vector<float> x (n);
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, b3 = 0.0, b4 = 0.0, b5 = 0.0, b6 = 0.0;
    double peak = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double w = white[i];
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        const double pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
        b6 = w * 0.115926;
        x[i] = static_cast<float> (pink);
        peak = std::max (peak, std::abs (pink));
    }

    const float scale = (peak > 0.0) ? static_cast<float> (static_cast<double> (amplitude) / peak) : 0.0f;
    for (auto& v : x)
        v *= scale;
    return x;
}

/** Sine wave of `n` samples at `freqHz`. */
inline std::vector<float> sineWave (size_t n, double freqHz, double sampleRate, float amplitude)
{
    std::vector<float> x (n);
    const double w = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
    for (size_t i = 0; i < n; ++i)
        x[i] = amplitude * static_cast<float> (std::sin (w * static_cast<double> (i)));
    return x;
}

//==============================================================================
// Continuity metrics (profile-switch tests)
//==============================================================================

/** Largest |x[i+1] - x[i]| for i in [from, to - 1), bounds clamped to the signal. */
inline double maxAbsStep (const std::vector<float>& x, size_t from, size_t to)
{
    to = std::min (to, x.size());
    double worst = 0.0;
    for (size_t i = from; i + 1 < to; ++i)
        worst = std::max (worst, static_cast<double> (std::abs (x[i + 1] - x[i])));
    return worst;
}

/** RMS of x over [from, to), bounds clamped to the signal. 0 for an empty range. */
inline double windowRms (const std::vector<float>& x, size_t from, size_t to)
{
    to = std::min (to, x.size());
    if (from >= to)
        return 0.0;
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += static_cast<double> (x[i]) * static_cast<double> (x[i]);
    return std::sqrt (sum / static_cast<double> (to - from));
}

/** Smallest windowed RMS over windows of `window` samples slid by window / 4 across
    [from, to). At least one window (starting at `from`) is always evaluated. */
inline double minWindowRms (const std::vector<float>& x, size_t from, size_t to, size_t window)
{
    to = std::min (to, x.size());
    const size_t hop = std::max<size_t> (window / 4, 1);
    double lowest = std::numeric_limits<double>::infinity();
    size_t start = from;
    do
    {
        lowest = std::min (lowest, windowRms (x, start, start + window));
        start += hop;
    }
    while (start + window <= to);
    return lowest;
}

/** Worst sample-to-sample step in [switch - 2048, switch + transition + 4096) divided by the
    larger of the worst step in the pre-steady window [switch - 16384, switch - 2048) and in the
    post-steady window [switch + transition + 4096, + 8192 more). Worst of the two channels.
    Callers must leave 16384 samples before the switch and 12288 after the transition. A
    steady level of zero reports +infinity. */
inline double switchStepRatio (const StereoSignal& s, size_t switchSample, size_t transitionSamples)
{
    double worstRatio = 0.0;
    for (const std::vector<float>* ch : { &s.left, &s.right })
    {
        const double around = maxAbsStep (*ch, switchSample - 2048, switchSample + transitionSamples + 4096);
        const double pre = maxAbsStep (*ch, switchSample - 16384, switchSample - 2048);
        const double post = maxAbsStep (*ch, switchSample + transitionSamples + 4096,
                                        switchSample + transitionSamples + 4096 + 8192);
        const double steady = std::max (pre, post);
        const double ratio = (steady > 0.0) ? around / steady : std::numeric_limits<double>::infinity();
        worstRatio = std::max (worstRatio, ratio);
    }
    return worstRatio;
}

/** Smallest windowed RMS inside the transition window [switch, switch + transition) divided by
    the smaller of the pre-steady RMS [switch - 16384, switch - 2048) and the post-steady RMS
    [switch + transition + 4096, + 8192 more). Worst of the two channels. Same margins as
    switchStepRatio. */
inline double switchMinRmsRatio (const StereoSignal& s, size_t switchSample, size_t transitionSamples,
                                  size_t window = 1024)
{
    double worstRatio = std::numeric_limits<double>::infinity();
    for (const std::vector<float>* ch : { &s.left, &s.right })
    {
        const double inside = minWindowRms (*ch, switchSample, switchSample + transitionSamples, window);
        const double pre = windowRms (*ch, switchSample - 16384, switchSample - 2048);
        const double post = windowRms (*ch, switchSample + transitionSamples + 4096,
                                       switchSample + transitionSamples + 4096 + 8192);
        const double steady = std::min (pre, post);
        const double ratio = (steady > 0.0) ? inside / steady : 0.0;
        worstRatio = std::min (worstRatio, ratio);
    }
    return worstRatio;
}

//==============================================================================
// Convolution oracle and block plans (BUG-02, D-12)
//==============================================================================

/** Double-precision linear convolution of x with h, truncated to x.size() samples. */
inline std::vector<double> directConvolve (const std::vector<float>& x, const std::vector<float>& h)
{
    std::vector<double> y (x.size(), 0.0);
    for (size_t n = 0; n < x.size(); ++n)
    {
        const size_t taps = std::min (h.size(), n + 1);
        double acc = 0.0;
        for (size_t k = 0; k < taps; ++k)
            acc += static_cast<double> (h[k]) * static_cast<double> (x[n - k]);
        y[n] = acc;
    }
    return y;
}

/** The seven block plans every block-size test runs: four uniform sizes, an odd size, and
    two irregular plans that include one-sample and mixed blocks. */
inline std::vector<std::vector<int>> blockPlans()
{
    return { { 32 }, { 64 }, { 128 }, { 512 }, { 37 }, { 1, 7, 300, 512, 3, 64 }, { 32, 512, 32, 64, 512 } };
}

} // namespace spatialcore::test
