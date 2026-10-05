#pragma once

// DemoComponent -- the SpatialCoreDemo worked example (Phase 4, D-01 / D-02 / D-12).
//
// This is what a plugin writes around SpatialCore: control routes end in an object position, the
// message thread keeps that position in per-object atomics, and the audio callback reads the
// atomics and calls RenderEngine::renderBlock with both opt-in flags set (engineComputesGains and
// engineDerivesDispatch, D-18). The glue lives here, in the consumer, never in the library (D-12).
//
// Threads (DR-1): everything except renderAudio() runs on the message thread. renderAudio() only
// loads atomics, writes buffers sized in prepareRender() and calls renderBlock(); it takes no
// lock, allocates nothing and logs nothing.

#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <SpatialCore/UI/SpatialMapComponent.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <vector>

class DemoComponent : public juce::Component,
                      private spatialcore::SpatialMapComponent::Listener
{
public:
    static constexpr int kNumObjects = 4;

    DemoComponent();
    ~DemoComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Overwrites one object: the atomics the audio callback reads, and the map. Message thread.
    void setObject (int index, float azimuthDeg, float elevationDeg, float distance, bool enabled);

    // Sizes every buffer the audio path uses and prepares the engine for Binaural output.
    // Message thread, before audio starts (or before an offline render).
    void prepareRender (double sampleRate, int maxBlock);

    // The audio-thread entry point: one test tone per enabled object, rendered through
    // RenderEngine::renderBlock to the first two output channels (the rest are zeroed).
    void renderAudio (float* const* out, int numChannels, int numSamples);

    // Offline: renders four blocks with only `index` enabled and returns the left/right RMS ratio
    // of the last block, then restores every enabled flag. It shares the engine with
    // renderAudio(), so it is never called while an audio device is running.
    float measureLeftRight (int index);

    // Offline: writes before-drag.png and after-drag.png into `dir` (D-03) and appends one report
    // line per image to `report`. Returns false when a file cannot be written.
    bool writeDragScreenshots (const juce::File& dir, juce::String& report);

    float getAzimuth (int i) const   { return az_[(size_t) i].load (std::memory_order_relaxed); }
    float getElevation (int i) const { return el_[(size_t) i].load (std::memory_order_relaxed); }
    float getDistance (int i) const  { return dist_[(size_t) i].load (std::memory_order_relaxed); }
    bool  isEnabled (int i) const    { return enabled_[(size_t) i].load (std::memory_order_relaxed); }

    spatialcore::SpatialMapComponent& getMap() { return map_; }

protected:
    // Map callbacks (message thread). Elevation is kept: the map edits azimuth and distance only.
    void objectPositionChanged (int objectIndex, float azimuthDeg, float distance) override;
    void objectSelected (int objectIndex) override;

    int selectedObject_ = 0;

private:
    bool writePng (const juce::File& file);
    juce::String describe (const char* label, float leftRight) const;

    spatialcore::SMLLookAndFeel lookAndFeel_;
    spatialcore::SpatialMapComponent map_ { kNumObjects };

    std::array<std::atomic<float>, kNumObjects> az_ {};
    std::array<std::atomic<float>, kNumObjects> el_ {};
    std::array<std::atomic<float>, kNumObjects> dist_ {};
    std::array<std::atomic<bool>, kNumObjects> enabled_ {};

    // Sized in prepareRender().
    spatialcore::RenderEngine engine_;
    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    std::vector<float> tapFade_;
    std::array<std::vector<float>, kNumObjects> tone_;
    std::array<std::vector<float>, kNumObjects> distGain_;
    std::array<double, kNumObjects> tonePhase_ {};
    std::vector<float> offlineLeft_, offlineRight_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DemoComponent)
};
