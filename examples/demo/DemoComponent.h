#pragma once

// DemoComponent -- the SpatialCoreDemo worked example (Phase 4, D-01 / D-02 / D-12).
//
// This is what a plugin writes around SpatialCore. Three control routes end in an object position:
//   - ADM-OSC in (ADMOSCReceiver::Listener): positions merge in with the NaN rule, and position
//     queries are answered through ADMOSCSender::queueReply (D-08a);
//   - trajectories: a 60 Hz timer ticks TrajectoryEngine, setting TrajectoryState::reverse itself
//     because getState() always reports false (RESEARCH Pitfall 3), and ticks ADMOSCSender;
//   - the spatial map: a drag arrives through SpatialMapComponent::Listener.
// The message thread keeps the position in per-object atomics. The audio callback reads the atomics
// and calls RenderEngine::renderBlock with both opt-in flags set (engineComputesGains and
// engineDerivesDispatch, D-18). The glue lives here, in the consumer, never in the library (D-12).
//
// Threads (DR-1): everything except renderAudio() and the audio callback runs on the message
// thread. renderAudio() only loads atomics, writes buffers sized in prepareRender() and calls
// renderBlock(); it takes no lock, allocates nothing and logs nothing. TrajectoryEngine is never
// read there: its output is copied into the atomics on the message thread (that cross-thread read
// is RTSF-02, Phase 5, and the demo avoids it).

#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <SpatialCore/OSC/ADMOSCSender.h>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <SpatialCore/UI/SpatialMapComponent.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class DemoComponent : public juce::Component,
                      public juce::AudioIODeviceCallback,
                      private spatialcore::SpatialMapComponent::Listener,
                      private spatialcore::ADMOSCReceiver::Listener,
                      private juce::Timer
{
public:
    static constexpr int kNumObjects = 4;

    DemoComponent();
    ~DemoComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Overwrites one object: its trajectory origin, the atomics the audio callback reads, and the
    // map. Message thread.
    void setObject (int index, float azimuthDeg, float elevationDeg, float distance, bool enabled);

    // The scene the screenshots and the interactive window start from: object 1 above, object 2
    // below, object 3 at ear level, object 4 off.
    void setDefaultScene();

    // Hides the shape / speed / reverse row so the map fills the component (screenshots).
    void setControlsVisible (bool visible);

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

    //==========================================================================
    // Interactive wiring (message thread).

    // Opens the default audio device and starts rendering. Returns an error message, or empty.
    juce::String startAudio();

    // Connects ADM-OSC in on `inPort` and out to 127.0.0.1:`outPort`. Returns false and connects
    // nothing when the two ports would feed the demo its own broadcasts (oscPortsConflict) or a
    // socket cannot be opened.
    bool startOsc (int inPort, int outPort);
    void stopOsc();

    // The 60 Hz timer: trajectories, OSC send, map animation.
    void startUpdates();
    void stopUpdates();

    // One trajectory step for every object whose shape is not None; the timer calls it with 1/60 s.
    void stepTrajectories (float dt);

    // Headless check of the three control routes and the query reply, on a pair of free loopback
    // ports chosen by the OS. Prints one SELFTEST line per check, then SELFTEST RESULT; `done` receives
    // the overall result. Needs the message loop to be running.
    void runSelfTest (std::function<void (bool)> done);

    float getAzimuth (int i) const   { return az_[(size_t) i].load (std::memory_order_relaxed); }
    float getElevation (int i) const { return el_[(size_t) i].load (std::memory_order_relaxed); }
    float getDistance (int i) const  { return dist_[(size_t) i].load (std::memory_order_relaxed); }
    bool  isEnabled (int i) const    { return enabled_[(size_t) i].load (std::memory_order_relaxed); }

    spatialcore::SpatialMapComponent& getMap() { return map_; }

    // juce::AudioIODeviceCallback
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    // SpatialMapComponent::Listener (message thread). Elevation is kept: the map edits azimuth and
    // distance only (D-04).
    void objectPositionChanged (int objectIndex, float azimuthDeg, float distance) override;
    void objectSelected (int objectIndex) override;

    // ADMOSCReceiver::Listener (message thread).
    void admPositionReceived (int objectIndex, float azimuthDeg, float elevationDeg, float distance) override;
    void admPositionQueried (int objectIndex, spatialcore::ADMPositionQuery kind) override;

    void timerCallback() override;

    struct Origin { float az = 0.0f, el = 0.0f, dist = 0.5f; };

    void storePosition (int index, float azimuthDeg, float elevationDeg, float distance);
    void showObjectInControls (int index);
    void sendOsc();
    void dragObject (int index, float azimuthDeg, float distance);
    bool writePng (const juce::File& file);
    juce::String describe (const char* label, float leftRight) const;

    // Self-test steps, chained through Timer::callAfterDelay (guarded by a WeakReference) so the
    // message loop delivers OSC.
    struct SelfTestRig;
    void selfTestReport (const juce::String& check, bool ok, const juce::String& detail);
    void selfTestFinish();
    void selfTestOsc();
    void selfTestQuery();
    void selfTestTrajectory();
    void selfTestMap();

    spatialcore::SMLLookAndFeel lookAndFeel_;
    spatialcore::SpatialMapComponent map_ { kNumObjects };

    // Message-thread control row for the selected object.
    juce::ComboBox shapeBox_;
    juce::Slider speedSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton reverseToggle_ { "Reverse" };
    bool controlsVisible_ = true;
    int selectedObject_ = 0;

    // The audio thread reads these; the message thread writes them. Each object's azimuth,
    // elevation and distance are three separate relaxed atomics, so a block that starts while the
    // message thread is mid-update can pair the new azimuth with the previous distance for that one
    // block (IN-06b). That is inaudible for a demo's tone sources and costs no lock. A consumer that
    // needs the triple to be consistent should publish it through a seqlock or a triple buffer (as
    // RenderEngine does for its layout), never through a mutex on the audio thread.
    std::array<std::atomic<float>, kNumObjects> az_ {};
    std::array<std::atomic<float>, kNumObjects> el_ {};
    std::array<std::atomic<float>, kNumObjects> dist_ {};
    std::array<std::atomic<bool>, kNumObjects> enabled_ {};

    // Message-thread only.
    std::array<Origin, kNumObjects> origin_ {};
    std::array<int, kNumObjects> shape_ {};
    std::array<float, kNumObjects> speed_ {};
    std::array<bool, kNumObjects> reverse_ {};
    spatialcore::TrajectoryEngine trajectory_;
    spatialcore::ADMOSCReceiver oscIn_;
    spatialcore::ADMOSCSender oscOut_;
    juce::AudioDeviceManager deviceManager_;

    // Sized in prepareRender().
    spatialcore::RenderEngine engine_;
    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    std::vector<float> tapFade_;
    std::array<std::vector<float>, kNumObjects> tone_;
    std::array<std::vector<float>, kNumObjects> distGain_;
    std::array<double, kNumObjects> tonePhase_ {};
    std::vector<float> offlineLeft_, offlineRight_;

    // Self-test state.
    std::unique_ptr<SelfTestRig> rig_;
    std::function<void (bool)> selfTestDone_;
    juce::StringArray selfTestFailures_;

    // Lets the delayed self-test steps check the component is still alive before they run.
    JUCE_DECLARE_WEAK_REFERENCEABLE (DemoComponent)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DemoComponent)
};
