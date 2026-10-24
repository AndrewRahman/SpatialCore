#pragma once

// HarnessProcessor / HarnessEditor -- the smallest plugin that drives SpatialCore the way
// OpenSpatialDelay will. This is the copy-paste reference for that migration: the only SpatialCore
// includes are the two umbrellas, and every opt-in facade feature is used (see processBlock).

#include <SpatialCore/SpatialCore.h>
#include <SpatialCore/SpatialCoreUI.h>

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <vector>

class HarnessProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kNumObjects = 2;
    static constexpr int kMaxChannels = 64;

    HarnessProcessor();
    ~HarnessProcessor() override = default;

    // Message-thread setters. Each one only forwards to the engine (which is lock-free or
    // message-thread by contract) and remembers the value for the saved state.
    void setOutputFormat (spatialcore::OutputFormat format);
    void setAlgorithmIndex (int index);
    void setHRTFProfile (int profileIndex);

    // Any thread: the editor and a host automation thread write, the audio thread reads.
    void setObjectPosition (int object, float azimuthDeg, float elevationDeg, float distance);
    void getObjectPosition (int object, float& azimuthDeg, float& elevationDeg, float& distance) const noexcept;

    // The format the most recent block actually rendered (index into OutputFormat).
    int getLastRenderedFormat() const noexcept { return lastRenderedFormat_.load (std::memory_order_relaxed); }

    // The sound check reads the engine's diagnostics through this.
    spatialcore::RenderEngine& getEngine() noexcept { return engine_; }

    //==========================================================================
    const juce::String getName() const override { return "SpatialCore Harness"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    spatialcore::RenderEngine engine_;

    // Message thread writes, audio thread reads: std::atomic, never a lock.
    std::array<std::atomic<float>, kNumObjects> objectAzimuth_;
    std::array<std::atomic<float>, kNumObjects> objectElevation_;
    std::array<std::atomic<float>, kNumObjects> objectDistance_;
    std::atomic<int> lastRenderedFormat_ { 0 };

    // The values the saved state holds (message thread only).
    int formatIndex_ = static_cast<int> (spatialcore::OutputFormat::Binaural);
    int algorithmIndex_ = spatialcore::kAlgorithmIndexVBAP;
    int hrtfProfile_ = spatialcore::kAlwaysEmbeddedHRTFProfile;

    // Every buffer is sized in prepareToPlay, so processBlock never allocates.
    int preparedBlockSize_ = 0;
    double preparedSampleRate_ = 48000.0;
    std::array<std::vector<float>, kNumObjects> monoBuffers_;
    std::array<std::vector<float>, kNumObjects> distanceGains_;
    std::vector<float> tapFade_;
    std::array<std::vector<float>, kMaxChannels> renderBank_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarnessProcessor)
};

//==============================================================================
// The editor: the shared spatial map, drawn with the shared SML look-and-feel.
class HarnessEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit HarnessEditor (HarnessProcessor& owner);
    ~HarnessEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    HarnessProcessor& processor_;

    // One look-and-feel for every open editor (the SMLLookAndFeel loads its typefaces per
    // instance, so a consumer creates one and shares it, D-12).
    juce::SharedResourcePointer<spatialcore::SMLLookAndFeel> lookAndFeel_;
    spatialcore::SpatialMapComponent map_ { HarnessProcessor::kNumObjects };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarnessEditor)
};
