// HarnessCheck -- the headless sound check for the consumer harness (D-04). No window, no audio
// device, no DAW: it drives HarnessProcessor::processBlock the way a host would and the exit code is
// the verdict (0 only when every check passes).

#include "HarnessPlugin.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;

// The check must stay honest when the harness is built with -ffast-math (the OpenSpatialDelay-like
// configuration): std::isfinite folds to true there. Read the exponent bits instead.
bool isFiniteSample (float x) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &x, sizeof (bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

struct Verdict
{
    int failures = 0;

    void report (bool ok, const juce::String& name, const juce::String& detail = {})
    {
        std::cout << (ok ? "PASS " : "FAIL ") << name << (detail.isEmpty() ? "" : " : ") << detail << std::endl;
        if (! ok)
            ++failures;
    }
};

// Renders blocks of a 220 Hz, 0.25 amplitude tone through the processor and measures the result.
struct Rig
{
    explicit Rig (HarnessProcessor& p) : processor (p), buffer (HarnessProcessor::kMaxChannels, kBlockSize) {}

    struct BlockResult
    {
        bool allFinite = true;
        float firstSampleCh0 = 0.0f;
        float lastSampleCh0 = 0.0f;
        double energyPerSample = 0.0;   // summed over the measured channels
    };

    // Renders one block. Channel 0 carries the input tone in and the first output channel out.
    BlockResult renderBlock (int measuredChannels)
    {
        for (int n = 0; n < kBlockSize; ++n)
        {
            buffer.setSample (0, n, 0.25f * std::sin (static_cast<float> (phase)));
            phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / kSampleRate;
        }
        if (phase > 1.0e6)
            phase = std::fmod (phase, 2.0 * juce::MathConstants<double>::pi);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        BlockResult r;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const float* data = buffer.getReadPointer (c);
            for (int n = 0; n < kBlockSize; ++n)
                if (! isFiniteSample (data[n]))
                    r.allFinite = false;
        }

        r.firstSampleCh0 = buffer.getSample (0, 0);
        r.lastSampleCh0 = buffer.getSample (0, kBlockSize - 1);

        for (int c = 0; c < measuredChannels && c < buffer.getNumChannels(); ++c)
        {
            const float* data = buffer.getReadPointer (c);
            double sum = 0.0;
            for (int n = 0; n < kBlockSize; ++n)
                sum += static_cast<double> (data[n]) * static_cast<double> (data[n]);
            r.energyPerSample += sum / kBlockSize;
        }
        return r;
    }

    HarnessProcessor& processor;
    juce::AudioBuffer<float> buffer;
    double phase = 0.0;
};

int runCheck()
{
    Verdict verdict;

    HarnessProcessor processor;

    // A machine-wide HRTF override in the shared folder must not change the reported source, so the
    // engine looks in an empty temporary folder instead.
    const auto emptyFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("sc-harness-empty-hrtf-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    emptyFolder.createDirectory();
    processor.getEngine().setSharedHRTFFolderForTesting (emptyFolder);

    processor.setPlayConfigDetails (2, HarnessProcessor::kMaxChannels, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);

    const bool idle = processor.getEngine().waitForHRTFProfileIdle (20000);
    verdict.report (idle, "HRTF loader idle within 20 s");

    processor.setObjectPosition (0, 30.0f, 0.0f, 1.0f);
    processor.setObjectPosition (1, -30.0f, 0.0f, 1.0f);

    Rig rig (processor);

    // The tracer slice: 64 binaural blocks through processBlock.
    bool finite = true;
    double lastEnergy = 0.0;
    for (int b = 0; b < 64; ++b)
    {
        const auto r = rig.renderBlock (2);
        finite = finite && r.allFinite;
        if (b >= 48)
            lastEnergy += r.energyPerSample / 16.0;
    }
    const double rms = std::sqrt (lastEnergy);
    verdict.report (finite, "binaural: every sample finite");
    verdict.report (rms > 1.0e-4, "binaural: not silent", "rms " + juce::String (rms, 5));

    const auto status = processor.getEngine().getHRTFProfileStatus();
    const bool profileOk = status.activeProfile == spatialcore::kAlwaysEmbeddedHRTFProfile
                           && status.state == spatialcore::HRTFLoadState::Ready
                           && status.source == spatialcore::HRTFProfileSource::Embedded;
    verdict.report (profileOk, "HRTF profile 5 Ready from the embedded data",
                    spatialcore::describeHRTFProfileStatus (status));

    emptyFolder.deleteRecursively();

    const bool pass = verdict.failures == 0;
    std::cout << (pass ? "HarnessCheck: PASS" : "HarnessCheck: FAIL") << std::endl;
    return pass ? 0 : 1;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return runCheck();
}
