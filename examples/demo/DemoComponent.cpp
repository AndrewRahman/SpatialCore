#include "DemoComponent.h"

#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/OSC/OSCPortValidation.h>
#include <algorithm>
#include <cmath>
#include <iostream>

#if JUCE_WINDOWS
 #include <winsock2.h>
#else
 #include <arpa/inet.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <unistd.h>
#endif

using namespace spatialcore;

namespace
{
// The shape list of TrajectoryEngine::computeTrajectory, in order. Shape 0 is None, 9 is Orbit.
const char* const kShapeNames[] = { "None", "Bounce", "Circle", "Cross", "Figure-8", "Heart", "Helix",
                                    "Infinity", "Line", "Orbit", "Random", "Spiral", "Square", "Triangle" };
constexpr int kNumShapes = (int) (sizeof (kShapeNames) / sizeof (kShapeNames[0]));
constexpr int kOrbit = 9;
constexpr int kControlsHeight = 40;

// A UDP port the OS reports as free, for the self-test's loopback pair (so it never collides
// with another run or another app on a fixed number). juce::DatagramSocket cannot bind port 0,
// so this binds a plain socket to port 0, reads the assigned port and closes it. 0 on failure.
int findFreeUdpPort()
{
   #if JUCE_WINDOWS
    WSADATA wsa;
    if (WSAStartup (MAKEWORD (2, 2), &wsa) != 0)
        return 0;
    const SOCKET s = ::socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
        return 0;
   #else
    const int s = ::socket (AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;
   #endif

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl (INADDR_ANY);

    int port = 0;
    if (::bind (s, reinterpret_cast<sockaddr*> (&addr), sizeof (addr)) == 0)
    {
       #if JUCE_WINDOWS
        int len = (int) sizeof (addr);
       #else
        socklen_t len = sizeof (addr);
       #endif
        if (::getsockname (s, reinterpret_cast<sockaddr*> (&addr), &len) == 0)
            port = (int) ntohs (addr.sin_port);
    }

   #if JUCE_WINDOWS
    ::closesocket (s);
   #else
    ::close (s);
   #endif
    return port;
}
} // namespace

//==============================================================================
// What the self-test needs besides the demo itself: a loopback OSC sender into the demo's
// receive port, and a receiver on the demo's send port that records what arrives.
struct DemoComponent::SelfTestRig : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
    juce::OSCSender sender;
    juce::OSCReceiver receiver;
    juce::CriticalSection lock;
    std::vector<juce::OSCMessage> received;

    void oscMessageReceived (const juce::OSCMessage& message) override
    {
        const juce::ScopedLock sl (lock);
        received.push_back (message);
    }
};

//==============================================================================
DemoComponent::DemoComponent()
{
    setLookAndFeel (&lookAndFeel_);
    setOpaque (true);

    for (int i = 0; i < kNumObjects; ++i)
    {
        az_[(size_t) i].store (0.0f);
        el_[(size_t) i].store (0.0f);
        dist_[(size_t) i].store (0.5f);
        enabled_[(size_t) i].store (false);
        shape_[(size_t) i] = 0;
        speed_[(size_t) i] = 1.0f;
        reverse_[(size_t) i] = false;
    }

    map_.addListener (this);
    map_.setTrajectoryEngine (&trajectory_);
    addAndMakeVisible (map_);

    for (int s = 0; s < kNumShapes; ++s)
        shapeBox_.addItem (kShapeNames[s], s + 1);
    shapeBox_.setSelectedId (1, juce::dontSendNotification);
    shapeBox_.onChange = [this]
    {
        const int shape = shapeBox_.getSelectedId() - 1;
        const auto i = (size_t) selectedObject_;
        shape_[i] = shape;
        if (shape == 0)
        {
            // Back to a fixed object: drop the engine's state and return to the origin.
            trajectory_.reset (selectedObject_, origin_[i].az, origin_[i].el, origin_[i].dist);
            map_.setTrajectoryState (selectedObject_, trajectory_.getState (selectedObject_));
            storePosition (selectedObject_, origin_[i].az, origin_[i].el, origin_[i].dist);
        }
    };
    addAndMakeVisible (shapeBox_);

    speedSlider_.setRange (0.1, 4.0, 0.05);
    speedSlider_.setValue (1.0, juce::dontSendNotification);
    speedSlider_.onValueChange = [this] { speed_[(size_t) selectedObject_] = (float) speedSlider_.getValue(); };
    addAndMakeVisible (speedSlider_);

    reverseToggle_.onClick = [this] { reverse_[(size_t) selectedObject_] = reverseToggle_.getToggleState(); };
    addAndMakeVisible (reverseToggle_);

    oscIn_.addListener (this);

    setDefaultScene();
    setSize (480, 480 + kControlsHeight);
}

DemoComponent::~DemoComponent()
{
    stopTimer();
    deviceManager_.removeAudioCallback (this);
    deviceManager_.closeAudioDevice();
    oscIn_.removeListener (this);
    oscIn_.disconnect();
    oscOut_.disconnect();
    map_.removeListener (this);
    setLookAndFeel (nullptr);
}

void DemoComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void DemoComponent::resized()
{
    auto area = getLocalBounds();

    if (controlsVisible_)
    {
        auto row = area.removeFromBottom (kControlsHeight).reduced (8, 6);
        shapeBox_.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (8);
        reverseToggle_.setBounds (row.removeFromRight (90));
        speedSlider_.setBounds (row);
    }

    map_.setBounds (area);
}

void DemoComponent::setControlsVisible (bool visible)
{
    controlsVisible_ = visible;
    shapeBox_.setVisible (visible);
    speedSlider_.setVisible (visible);
    reverseToggle_.setVisible (visible);
    resized();
}

//==============================================================================
void DemoComponent::storePosition (int index, float azimuthDeg, float elevationDeg, float distance)
{
    const auto i = (size_t) index;
    az_[i].store (azimuthDeg, std::memory_order_relaxed);
    el_[i].store (elevationDeg, std::memory_order_relaxed);
    dist_[i].store (distance, std::memory_order_relaxed);
    map_.setObjectState (index, azimuthDeg, elevationDeg, distance, enabled_[i].load (std::memory_order_relaxed));
}

void DemoComponent::setObject (int index, float azimuthDeg, float elevationDeg, float distance, bool enabled)
{
    if (index < 0 || index >= kNumObjects)
        return;

    origin_[(size_t) index] = { azimuthDeg, elevationDeg, distance };
    enabled_[(size_t) index].store (enabled, std::memory_order_relaxed);
    storePosition (index, azimuthDeg, elevationDeg, distance);
}

void DemoComponent::setDefaultScene()
{
    // One object above (drawn bright), one below (drawn faded), one at ear level.
    for (int i = 0; i < kNumObjects; ++i)
        setObject (i, 0.0f, 0.0f, 0.5f, false);
    setObject (0,    0.0f,  30.0f, 0.5f, true);
    setObject (1, -110.0f, -60.0f, 0.7f, true);
    setObject (2,  150.0f,   0.0f, 0.3f, true);
    objectSelected (0);
}

void DemoComponent::objectPositionChanged (int objectIndex, float azimuthDeg, float distance)
{
    if (objectIndex < 0 || objectIndex >= kNumObjects)
        return;

    // The map has already moved its own dot. A drag moves the trajectory origin too, so a running
    // trajectory carries on from where the object was dropped.
    const auto i = (size_t) objectIndex;
    origin_[i].az = azimuthDeg;
    origin_[i].dist = distance;
    az_[i].store (azimuthDeg, std::memory_order_relaxed);
    dist_[i].store (distance, std::memory_order_relaxed);
}

void DemoComponent::objectSelected (int objectIndex)
{
    selectedObject_ = juce::jlimit (0, kNumObjects - 1, objectIndex);
    map_.setSelectedObject (selectedObject_);
    showObjectInControls (selectedObject_);
}

void DemoComponent::showObjectInControls (int index)
{
    const auto i = (size_t) index;
    shapeBox_.setSelectedId (shape_[i] + 1, juce::dontSendNotification);
    speedSlider_.setValue (speed_[i], juce::dontSendNotification);
    reverseToggle_.setToggleState (reverse_[i], juce::dontSendNotification);
}

//==============================================================================
// ADM-OSC in. The consumer rule documented on ADMOSCReceiver::Listener::admPositionReceived: an
// axis that arrives as NaN was not sent in this message, so the stored value is kept.
void DemoComponent::admPositionReceived (int objectIndex, float azimuthDeg, float elevationDeg, float distance)
{
    if (objectIndex < 0 || objectIndex >= kNumObjects)
        return;

    auto& o = origin_[(size_t) objectIndex];
    if (! std::isnan (azimuthDeg))   o.az = azimuthDeg;
    if (! std::isnan (elevationDeg)) o.el = elevationDeg;
    if (! std::isnan (distance))     o.dist = distance;

    // A fixed object follows the message at once; a moving one carries on from the new origin.
    if (shape_[(size_t) objectIndex] == 0)
        storePosition (objectIndex, o.az, o.el, o.dist);
}

// A device asked for the object's position: answer from the atomics. The reply goes to the
// sender's configured host and port, not to wherever the query came from (D-20).
void DemoComponent::admPositionQueried (int objectIndex, ADMPositionQuery kind)
{
    if (objectIndex < 0 || objectIndex >= kNumObjects)
        return;

    oscOut_.queueReply (objectIndex, kind, getAzimuth (objectIndex), getElevation (objectIndex),
                        getDistance (objectIndex));
}

bool DemoComponent::startOsc (int inPort, int outPort)
{
    if (oscPortsConflict (inPort, outPort, "127.0.0.1"))
        return false;

    if (! oscIn_.connect (inPort))
        return false;

    if (! oscOut_.connect ("127.0.0.1", outPort))
    {
        oscIn_.disconnect();
        return false;
    }

    return true;
}

void DemoComponent::stopOsc()
{
    oscIn_.disconnect();
    oscOut_.disconnect();
}

//==============================================================================
void DemoComponent::startUpdates() { startTimerHz (60); }
void DemoComponent::stopUpdates()  { stopTimer(); }

void DemoComponent::stepTrajectories (float dt)
{
    for (int i = 0; i < kNumObjects; ++i)
    {
        const auto idx = (size_t) i;
        if (shape_[idx] == 0)
            continue;

        TrajectoryEngine::ObjectInput in;
        in.shape = shape_[idx];
        in.speed = speed_[idx];
        in.reverse = reverse_[idx];
        in.originAz = origin_[idx].az;
        in.originEl = origin_[idx].el;
        in.originDist = origin_[idx].dist;
        trajectory_.tick (i, in, dt);

        // Copy into the atomics here, on the message thread; the audio callback never reads the engine.
        storePosition (i, wrapAzimuth (trajectory_.getFinalAz (i)), trajectory_.getFinalEl (i),
                       trajectory_.getFinalDist (i));

        // getState() always reports reverse as false (RESEARCH Pitfall 3): the demo sets it.
        auto state = trajectory_.getState (i);
        state.reverse = reverse_[idx];
        map_.setTrajectoryState (i, state);
    }
}

void DemoComponent::sendOsc()
{
    float azs[kNumObjects], els[kNumObjects], dists[kNumObjects];
    bool on[kNumObjects];
    for (int i = 0; i < kNumObjects; ++i)
    {
        azs[i] = getAzimuth (i);
        els[i] = getElevation (i);
        dists[i] = getDistance (i);
        on[i] = isEnabled (i);
    }

    // The sender keeps its own 30 Hz clock, so a 60 Hz caller is fine (D-07).
    oscOut_.tick (azs, els, dists, on, kNumObjects);
}

void DemoComponent::timerCallback()
{
    constexpr float kDt = 1.0f / 60.0f;
    stepTrajectories (kDt);
    sendOsc();
    map_.advanceStarAnimation (kDt);
    map_.repaint();
}

//==============================================================================
juce::String DemoComponent::startAudio()
{
    const auto error = deviceManager_.initialiseWithDefaultDevices (0, 2);
    if (error.isNotEmpty())
        return error;

    deviceManager_.addAudioCallback (this);
    return {};
}

void DemoComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepareRender (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void DemoComponent::audioDeviceStopped() {}

void DemoComponent::audioDeviceIOCallbackWithContext (const float* const* /*inputChannelData*/,
                                                      int /*numInputChannels*/,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& /*context*/)
{
    renderAudio (outputChannelData, numOutputChannels, numSamples);
}

//==============================================================================
void DemoComponent::prepareRender (double sampleRate, int maxBlock)
{
    sampleRate_ = sampleRate;
    maxBlock_ = maxBlock;

    engine_.prepare (sampleRate, maxBlock);
    engine_.setOutputFormat (OutputFormat::Binaural);

    tapFade_.assign ((size_t) maxBlock, 1.0f);
    for (int i = 0; i < kNumObjects; ++i)
    {
        tone_[(size_t) i].assign ((size_t) maxBlock, 0.0f);
        distGain_[(size_t) i].assign ((size_t) maxBlock, 1.0f);
    }
    offlineLeft_.assign ((size_t) maxBlock, 0.0f);
    offlineRight_.assign ((size_t) maxBlock, 0.0f);
}

void DemoComponent::renderAudio (float* const* out, int numChannels, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = 0; c < numChannels; ++c)
        std::fill (out[c], out[c] + numSamples, 0.0f);

    if (maxBlock_ <= 0 || numChannels < 2)
        return;

    constexpr double kTwoPi = 6.283185307179586;

    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (maxBlock_, numSamples - done);

        RenderSources sources;
        sources.numSamples = n;

        for (int i = 0; i < kNumObjects; ++i)
        {
            if (! enabled_[(size_t) i].load (std::memory_order_relaxed))
                continue;

            ObjectState state;
            state.azimuthDeg   = az_[(size_t) i].load (std::memory_order_relaxed);
            state.elevationDeg = el_[(size_t) i].load (std::memory_order_relaxed);
            state.distance     = dist_[(size_t) i].load (std::memory_order_relaxed);
            state.enabled      = true;

            // One tone per object, 220 Hz times the object number, phase kept between blocks.
            const double step = kTwoPi * 220.0 * (double) (i + 1) / sampleRate_;
            double phase = tonePhase_[(size_t) i];
            float* tone = tone_[(size_t) i].data();
            for (int s = 0; s < n; ++s)
            {
                tone[s] = 0.2f * (float) std::sin (phase);
                phase += step;
                if (phase >= kTwoPi)
                    phase -= kTwoPi;
            }
            tonePhase_[(size_t) i] = phase;

            // What a consumer supplies for the surround, HRTF and Ambisonics paths; the Simple
            // binaural path folds distance into its own gains.
            const float g = distanceAttenuation (state.distance);
            std::fill (distGain_[(size_t) i].begin(), distGain_[(size_t) i].begin() + n, g);

            sources.monoBuffers[i] = tone;
            sources.tapFadeGainPerSample[i] = tapFade_.data();
            sources.distGainPerSample[i] = distGain_[(size_t) i].data();
            sources.objectLive[i] = true;
            sources.objects[i] = state;
        }

        RenderBlockContext ctx;
        ctx.sampleRate = sampleRate_;
        ctx.engineComputesGains = true;    // D-18: the engine computes objGains / objChannelGains
        ctx.engineDerivesDispatch = true;  // D-18: the engine derives the dispatch from its layout

        float* chunk[2] = { out[0] + done, out[1] + done };
        engine_.renderBlock (sources, ctx, chunk, 2);

        done += n;
    }
}

float DemoComponent::measureLeftRight (int index)
{
    std::array<bool, kNumObjects> saved {};
    for (int i = 0; i < kNumObjects; ++i)
    {
        saved[(size_t) i] = enabled_[(size_t) i].load();
        enabled_[(size_t) i].store (i == index);
    }

    float* ptrs[2] = { offlineLeft_.data(), offlineRight_.data() };
    for (int b = 0; b < 4; ++b)
        renderAudio (ptrs, 2, maxBlock_);

    double sumL = 0.0, sumR = 0.0;
    for (int s = 0; s < maxBlock_; ++s)
    {
        sumL += (double) offlineLeft_[(size_t) s] * (double) offlineLeft_[(size_t) s];
        sumR += (double) offlineRight_[(size_t) s] * (double) offlineRight_[(size_t) s];
    }

    for (int i = 0; i < kNumObjects; ++i)
        enabled_[(size_t) i].store (saved[(size_t) i]);

    const double rmsL = std::sqrt (sumL / (double) maxBlock_);
    const double rmsR = std::sqrt (sumR / (double) maxBlock_);
    return (float) (rmsL / std::max (rmsR, 1e-9));
}

//==============================================================================
bool DemoComponent::writePng (const juce::File& file)
{
    const auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);

    juce::FileOutputStream stream (file);
    if (! stream.openedOk())
        return false;

    stream.setPosition (0);
    stream.truncate();

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream);
}

juce::String DemoComponent::describe (const char* label, float leftRight) const
{
    return juce::String (label) + ": object 1 az=" + juce::String (getAzimuth (0), 1)
         + " el=" + juce::String (getElevation (0), 1)
         + " d=" + juce::String (getDistance (0), 2)
         + " L/R=" + juce::String (leftRight, 2);
}

// Mouse-down on the object, one drag to azimuth `azimuthDeg` at `distance` of the radius, mouse-up.
// The events are built the way a real input source builds them.
void DemoComponent::dragObject (int index, float azimuthDeg, float distance)
{
    const float radius = (float) std::min (map_.getWidth(), map_.getHeight()) * 0.45f;
    const float cx = (float) map_.getWidth() * 0.5f;
    const float cy = (float) map_.getHeight() * 0.5f;
    const float azRad = juce::degreesToRadians (azimuthDeg);
    const juce::Point<float> to { cx - std::sin (azRad) * distance * radius,
                                  cy - std::cos (azRad) * distance * radius };
    const auto from = map_.getObjectScreenPos (index);

    auto makeEvent = [this] (juce::Point<float> pos, juce::Point<float> downPos, bool dragged)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                 pos,
                                 juce::ModifierKeys(),
                                 juce::MouseInputSource::defaultPressure,
                                 juce::MouseInputSource::defaultOrientation,
                                 juce::MouseInputSource::defaultRotation,
                                 juce::MouseInputSource::defaultTiltX,
                                 juce::MouseInputSource::defaultTiltY,
                                 &map_,
                                 &map_,
                                 juce::Time::getCurrentTime(),
                                 downPos,
                                 juce::Time::getCurrentTime(),
                                 1,
                                 dragged);
    };

    map_.mouseDown (makeEvent (from, from, false));
    map_.mouseDrag (makeEvent (to, from, true));
    map_.mouseUp (makeEvent (to, from, true));
}

bool DemoComponent::writeDragScreenshots (const juce::File& dir, juce::String& report)
{
    if (! dir.createDirectory().wasOk())
        return false;

    setControlsVisible (false);
    setSize (480, 480);
    setDefaultScene();

    if (! writePng (dir.getChildFile ("before-drag.png")))
        return false;
    report << describe ("before-drag", measureLeftRight (0)) << "\n";

    dragObject (0, 90.0f, 0.8f);

    if (! writePng (dir.getChildFile ("after-drag.png")))
        return false;
    report << describe ("after-drag", measureLeftRight (0)) << "\n";

    return true;
}

//==============================================================================
// Self-test. Each step prints one line and the next one is scheduled 300 ms later, so the running
// message loop delivers the OSC datagrams.
void DemoComponent::selfTestReport (const juce::String& check, bool ok, const juce::String& detail)
{
    std::cout << (ok ? "SELFTEST PASS " : "SELFTEST FAIL ") << check.toStdString();
    if (detail.isNotEmpty())
        std::cout << " " << detail.toStdString();
    std::cout << std::endl;

    if (! ok)
        selfTestFailures_.add (check);
}

void DemoComponent::runSelfTest (std::function<void (bool)> done)
{
    selfTestDone_ = std::move (done);
    selfTestFailures_.clear();

    for (int i = 0; i < kNumObjects; ++i)
    {
        shape_[(size_t) i] = 0;
        setObject (i, 0.0f, 0.0f, 0.5f, true);
    }

    rig_ = std::make_unique<SelfTestRig>();
    const int inPort = findFreeUdpPort();
    int outPort = findFreeUdpPort();
    while (outPort == inPort && outPort != 0)
        outPort = findFreeUdpPort();

    const bool wired = inPort > 0 && outPort > 0
                    && startOsc (inPort, outPort)
                    && rig_->sender.connect ("127.0.0.1", inPort)
                    && rig_->receiver.connect (outPort);
    if (! wired)
    {
        selfTestReport ("osc", false, "could not open a loopback port pair (" + juce::String (inPort)
                                          + " and " + juce::String (outPort) + ")");
        selfTestFinish();
        return;
    }
    rig_->receiver.addListener (rig_.get());

    // The 60 Hz timer ticks the sender, which flushes the query reply.
    startUpdates();

    rig_->sender.send ("/adm/obj/1/aed", 90.0f, 0.0f, 0.5f);
    juce::Timer::callAfterDelay (300, [this] { selfTestOsc(); });
}

void DemoComponent::selfTestOsc()
{
    const float lr = measureLeftRight (0);
    const bool ok = std::abs (getAzimuth (0) - 90.0f) < 0.5f && lr >= 2.0f;
    selfTestReport ("osc", ok, "az=" + juce::String (getAzimuth (0), 1) + " L/R=" + juce::String (lr, 2));

    // No arguments: a query for the object's position. xyz, because the sender's own position
    // broadcasts use /aed and would otherwise be indistinguishable from a reply.
    rig_->sender.send (juce::OSCMessage ("/adm/obj/1/xyz"));
    juce::Timer::callAfterDelay (300, [this] { selfTestQuery(); });
}

void DemoComponent::selfTestQuery()
{
    bool found = false;
    juce::String detail = "no reply";
    {
        const juce::ScopedLock sl (rig_->lock);
        for (const auto& m : rig_->received)
        {
            if (m.getAddressPattern().toString() != "/adm/obj/1/xyz" || m.size() != 3
                || ! m[0].isFloat32() || ! m[1].isFloat32() || ! m[2].isFloat32())
                continue;

            const float x = m[0].getFloat32(), y = m[1].getFloat32(), z = m[2].getFloat32();
            detail = "xyz=(" + juce::String (x, 2) + ", " + juce::String (y, 2) + ", " + juce::String (z, 2) + ")";
            found = std::abs (x + 0.5f) < 0.02f && std::abs (y) < 0.02f && std::abs (z) < 0.02f;
            break;
        }
    }
    selfTestReport ("query", found, detail);

    // The trajectory and map checks are synchronous; stop the timer so it cannot also step them.
    stopUpdates();
    selfTestTrajectory();
}

void DemoComponent::selfTestTrajectory()
{
    // Object 2 at the origin, Orbit forward: 15 steps of 1/60 s at speed 1 is a quarter turn, to
    // the left. Then the same from a reset, reversed, is a quarter turn to the right.
    constexpr float kDt = 1.0f / 60.0f;
    const int obj = 1;

    shape_[(size_t) obj] = kOrbit;
    speed_[(size_t) obj] = 1.0f;

    setObject (obj, 0.0f, 0.0f, 0.5f, true);
    trajectory_.reset (obj, 0.0f, 0.0f, 0.5f);
    reverse_[(size_t) obj] = false;
    for (int s = 0; s < 15; ++s)
        stepTrajectories (kDt);
    const float forwardAz = getAzimuth (obj);
    const float forwardLr = measureLeftRight (obj);

    setObject (obj, 0.0f, 0.0f, 0.5f, true);
    trajectory_.reset (obj, 0.0f, 0.0f, 0.5f);
    reverse_[(size_t) obj] = true;
    for (int s = 0; s < 15; ++s)
        stepTrajectories (kDt);
    const float reverseAz = getAzimuth (obj);
    const float reverseLr = measureLeftRight (obj);

    shape_[(size_t) obj] = 0;
    reverse_[(size_t) obj] = false;
    trajectory_.reset (obj, 0.0f, 0.0f, 0.5f);

    const bool ok = std::abs (forwardAz - 90.0f) < 2.0f && forwardLr >= 2.0f
                 && std::abs (reverseAz + 90.0f) < 2.0f && reverseLr <= 0.5f;
    selfTestReport ("trajectory", ok,
                    "forward az=" + juce::String (forwardAz, 1) + " L/R=" + juce::String (forwardLr, 2)
                    + " reverse az=" + juce::String (reverseAz, 1) + " L/R=" + juce::String (reverseLr, 2));

    selfTestMap();
}

void DemoComponent::selfTestMap()
{
    const int obj = 2;
    setObject (obj, 0.0f, 0.0f, 0.5f, true);

    // The other objects sit at the same spot after the earlier checks, so select this one first:
    // the map tests the selected object first, as it is drawn on top.
    objectSelected (obj);
    dragObject (obj, 90.0f, 0.8f);
    const float lr = measureLeftRight (obj);
    const bool ok = std::abs (getAzimuth (obj) - 90.0f) < 1.0f && lr >= 2.0f;
    selfTestReport ("map", ok, "az=" + juce::String (getAzimuth (obj), 1) + " L/R=" + juce::String (lr, 2));

    selfTestFinish();
}

void DemoComponent::selfTestFinish()
{
    const bool pass = selfTestFailures_.isEmpty();
    std::cout << "SELFTEST RESULT " << (pass ? "PASS" : "FAIL");
    if (! pass)
        std::cout << " (" << selfTestFailures_.joinIntoString (", ").toStdString() << ")";
    std::cout << std::endl;

    stopUpdates();
    stopOsc();
    if (rig_ != nullptr)
    {
        rig_->receiver.removeListener (rig_.get());
        rig_->receiver.disconnect();
        rig_->sender.disconnect();
    }

    if (selfTestDone_)
        selfTestDone_ (pass);
}
