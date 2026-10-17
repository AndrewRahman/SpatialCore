// HarnessCheck -- the headless sound check for the consumer harness (D-04). No window, no audio
// device, no DAW: it drives HarnessProcessor::processBlock the way a host would, and the exit code is
// the verdict (0 only when every check passes). Each check prints one PASS or FAIL line.
//
//   1. HRTF       profile 5 (MIT KEMAR) is Ready from the embedded data
//   2. Formats    every output format renders finite, non-silent sound; the block layout reports the
//                 requested format once the switch has settled; each switch fades out to an exact 0
//                 and back in from an exact 0 (engineFadesFormatSwitch)
//   3. Algorithms all twelve algorithm indices are non-silent on 7.1.4 and on Stereo
//   4. Moving     an object moving on Binaural stays finite and audible (HRIR updates)
//   5. Counters   no block ran on unsupplied gains, no block was longer than the prepared size
//   6. Editor     the editor paints with the shared SML look-and-feel

#include "HarnessPlugin.h"

#include <algorithm>
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
constexpr double kSilenceFloor = 1.0e-4;

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
        float firstSampleCh0 = 1.0f;
        float lastSampleCh0 = 1.0f;
        double energyPerSample = 0.0;   // mean square, summed over the measured channels
    };

    // Renders one block. Channel 0 carries the input tone in and the first output channel out.
    BlockResult renderBlock (int measuredChannels)
    {
        for (int n = 0; n < kBlockSize; ++n)
        {
            buffer.setSample (0, n, 0.25f * std::sin (static_cast<float> (phase)));
            phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / kSampleRate;
        }
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

    struct Run
    {
        bool allFinite = true;
        double rms = 0.0;   // over the last `tailBlocks` blocks, summed over the measured channels
    };

    Run renderBlocks (int numBlocks, int measuredChannels, int tailBlocks)
    {
        Run run;
        double energy = 0.0;
        for (int b = 0; b < numBlocks; ++b)
        {
            const auto r = renderBlock (measuredChannels);
            run.allFinite = run.allFinite && r.allFinite;
            if (b >= numBlocks - tailBlocks)
                energy += r.energyPerSample / tailBlocks;
        }
        run.rms = std::sqrt (energy);
        return run;
    }

    HarnessProcessor& processor;
    juce::AudioBuffer<float> buffer;
    double phase = 0.0;
};

int channelsOf (spatialcore::OutputFormat format)
{
    return std::min (spatialcore::OutputFormatRegistry::getInfo (format).requiredChannels, HarnessProcessor::kMaxChannels);
}

bool imageIsNotFlat (const juce::Image& image)
{
    if (! image.isValid() || image.getWidth() <= 0 || image.getHeight() <= 0)
        return false;

    const auto first = image.getPixelAt (0, 0);
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (image.getPixelAt (x, y) != first)
                return true;
    return false;
}

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

    //==========================================================================
    // 1. HRTF: the profile the constructor asked for (5, KEMAR) loads from the embedded data.
    verdict.report (processor.getEngine().waitForHRTFProfileIdle (20000), "HRTF loader idle within 20 s");

    processor.setObjectPosition (0, 30.0f, 0.0f, 1.0f);
    processor.setObjectPosition (1, -30.0f, 0.0f, 1.0f);

    Rig rig (processor);

    // 64 binaural blocks: long enough for the Simple to HRTF crossfade (4096 samples) to finish.
    {
        const auto run = rig.renderBlocks (64, 2, 16);
        verdict.report (run.allFinite, "binaural warm-up: every sample finite");
        verdict.report (run.rms > kSilenceFloor, "binaural warm-up: not silent", "rms " + juce::String (run.rms, 5));
    }

    const auto checkProfile = [&] (const juce::String& name)
    {
        const auto status = processor.getEngine().getHRTFProfileStatus();
        const bool ok = status.activeProfile == spatialcore::kAlwaysEmbeddedHRTFProfile
                        && status.state == spatialcore::HRTFLoadState::Ready
                        && status.source == spatialcore::HRTFProfileSource::Embedded;
        verdict.report (ok, name, spatialcore::describeHRTFProfileStatus (status));
    };
    checkProfile ("HRTF profile 5 Ready from the embedded data");

    //==========================================================================
    // 2. Every output format. Binaural is already active, so the first one needs no switch (and a
    // republish of the active format would only cost a fade dip).
    const int numFormats = spatialcore::NUM_OUTPUT_FORMATS;
    int formatsPassed = 0;
    for (int i = 0; i < numFormats; ++i)
    {
        const auto format = static_cast<spatialcore::OutputFormat> (i);
        const auto& info = spatialcore::OutputFormatRegistry::getInfo (format);
        const int channels = channelsOf (format);

        juce::StringArray problems;

        if (i != 0)
            processor.setOutputFormat (format);

        bool finite = true;
        double energy = 0.0;
        constexpr int kBlocks = 24, kTail = 8;
        for (int b = 0; b < kBlocks; ++b)
        {
            const auto r = rig.renderBlock (channels);
            finite = finite && r.allFinite;
            if (b >= kBlocks - kTail)
                energy += r.energyPerSample / kTail;

            if (i != 0)
            {
                // The switch fades the old format out to an exact 0 and the new one in from an exact 0.
                if (b == 0 && ! juce::exactlyEqual (r.lastSampleCh0, 0.0f))
                    problems.add ("fade-out ends at " + juce::String (r.lastSampleCh0, 8) + ", not 0");
                if (b == 1 && ! juce::exactlyEqual (r.firstSampleCh0, 0.0f))
                    problems.add ("fade-in starts at " + juce::String (r.firstSampleCh0, 8) + ", not 0");
            }
        }

        const double rms = std::sqrt (energy);
        if (! finite)
            problems.add ("non-finite sample");
        if (! (rms > kSilenceFloor))
            problems.add ("silent (rms " + juce::String (rms, 7) + ")");
        if (processor.getLastRenderedFormat() != i)
            problems.add ("block layout reports format " + juce::String (processor.getLastRenderedFormat()) + ", not " + juce::String (i));

        const juce::String name = juce::String ("format ") + juce::String (i) + " " + info.name;
        const juce::String detail = juce::String (channels) + " ch, rms " + juce::String (rms, 5);
        if (problems.isEmpty())
        {
            verdict.report (true, name, detail);
            ++formatsPassed;
        }
        else
        {
            verdict.report (false, name, detail + ", " + problems.joinIntoString ("; "));
        }
    }

    //==========================================================================
    // 3. All twelve algorithm indices, on a discrete speaker layout and on Stereo.
    int algorithmsPassed = 0;
    for (const auto format : { spatialcore::OutputFormat::Surround7_1_4, spatialcore::OutputFormat::Stereo })
    {
        processor.setOutputFormat (format);
        rig.renderBlocks (24, channelsOf (format), 4);

        int silent = 0;
        bool finite = true;
        double minRms = 1.0e9;
        for (int index = spatialcore::kAlgorithmIndexAmbisonics; index <= spatialcore::kAlgorithmIndexBlumlein; ++index)
        {
            processor.setAlgorithmIndex (index);
            const auto run = rig.renderBlocks (8, channelsOf (format), 4);
            finite = finite && run.allFinite;
            minRms = std::min (minRms, run.rms);
            if (! (run.rms > kSilenceFloor))
                ++silent;
        }
        processor.setAlgorithmIndex (spatialcore::kAlgorithmIndexVBAP);

        const bool ok = finite && silent == 0;
        verdict.report (ok, juce::String ("algorithms on ") + spatialcore::OutputFormatRegistry::getInfo (format).name,
                        "12 indices, " + juce::String (silent) + " silent, lowest rms " + juce::String (minRms, 5));
        if (ok)
            algorithmsPassed += 12;
    }

    //==========================================================================
    // 4. An object moving 3 degrees per block on Binaural exercises the HRIR updates.
    {
        processor.setOutputFormat (spatialcore::OutputFormat::Binaural);
        rig.renderBlocks (24, 2, 4);

        bool finite = true;
        double energy = 0.0;
        constexpr int kBlocks = 64;
        for (int b = 0; b < kBlocks; ++b)
        {
            processor.setObjectPosition (0, 30.0f + 3.0f * static_cast<float> (b), 0.0f, 1.0f);
            const auto r = rig.renderBlock (2);
            finite = finite && r.allFinite;
            energy += r.energyPerSample / kBlocks;
        }
        const double rms = std::sqrt (energy);
        verdict.report (finite && rms > kSilenceFloor, "moving object on Binaural", "rms " + juce::String (rms, 5));
    }

    checkProfile ("HRTF profile 5 still Ready from the embedded data after every switch");

    //==========================================================================
    // 5. Counters.
    {
        const int unsupplied = processor.getEngine().getUnsuppliedGainsBlockCount();
        const int oversized = processor.getEngine().getOversizedBlockCount();
        verdict.report (unsupplied == 0, "unsupplied-gains block count is 0", juce::String (unsupplied));
        verdict.report (oversized == 0, "oversized block count is 0", juce::String (oversized));
    }

    //==========================================================================
    // 6. The editor paints with the shared SML look-and-feel. It is deleted before the processor.
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        if (editor == nullptr)
        {
            verdict.report (false, "editor created");
        }
        else
        {
            editor->setSize (420, 420);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            verdict.report (imageIsNotFlat (image), "editor paints", juce::String (image.getWidth()) + " x " + juce::String (image.getHeight()));
            verdict.report (dynamic_cast<spatialcore::SMLLookAndFeel*> (&editor->getLookAndFeel()) != nullptr,
                            "editor uses the shared SML look-and-feel");
            editor.reset();
        }
    }

    emptyFolder.deleteRecursively();

    //==========================================================================
    if (verdict.failures == 0 && formatsPassed == numFormats)
    {
        std::cout << "HarnessCheck: PASS (" << numFormats << " formats, " << (algorithmsPassed / 2)
                  << " algorithms, profile " << spatialcore::kAlwaysEmbeddedHRTFProfile << " embedded)" << std::endl;
        return 0;
    }

    std::cout << "HarnessCheck: FAIL (" << verdict.failures << " failed checks)" << std::endl;
    return 1;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return runCheck();
}
