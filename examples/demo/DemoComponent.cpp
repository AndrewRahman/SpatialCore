#include "DemoComponent.h"

#include <SpatialCore/Core/SpatialMath.h>
#include <algorithm>
#include <cmath>

using namespace spatialcore;

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
    }

    map_.addListener (this);
    addAndMakeVisible (map_);
    setSize (480, 480);
}

DemoComponent::~DemoComponent()
{
    map_.removeListener (this);
    setLookAndFeel (nullptr);
}

void DemoComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void DemoComponent::resized()
{
    map_.setBounds (getLocalBounds());
}

//==============================================================================
void DemoComponent::setObject (int index, float azimuthDeg, float elevationDeg, float distance, bool enabled)
{
    if (index < 0 || index >= kNumObjects)
        return;

    az_[(size_t) index].store (azimuthDeg, std::memory_order_relaxed);
    el_[(size_t) index].store (elevationDeg, std::memory_order_relaxed);
    dist_[(size_t) index].store (distance, std::memory_order_relaxed);
    enabled_[(size_t) index].store (enabled, std::memory_order_relaxed);
    map_.setObjectState (index, azimuthDeg, elevationDeg, distance, enabled);
}

void DemoComponent::objectPositionChanged (int objectIndex, float azimuthDeg, float distance)
{
    if (objectIndex < 0 || objectIndex >= kNumObjects)
        return;

    az_[(size_t) objectIndex].store (azimuthDeg, std::memory_order_relaxed);
    dist_[(size_t) objectIndex].store (distance, std::memory_order_relaxed);
}

void DemoComponent::objectSelected (int objectIndex)
{
    selectedObject_ = objectIndex;
    map_.setSelectedObject (objectIndex);
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

bool DemoComponent::writeDragScreenshots (const juce::File& dir, juce::String& report)
{
    if (! dir.createDirectory().wasOk())
        return false;

    setSize (480, 480);

    // One object above (drawn bright), one below (drawn faded), one at ear level.
    for (int i = 0; i < kNumObjects; ++i)
        setObject (i, 0.0f, 0.0f, 0.5f, false);
    setObject (0,    0.0f,  30.0f, 0.5f, true);
    setObject (1, -110.0f, -60.0f, 0.7f, true);
    setObject (2,  150.0f,   0.0f, 0.3f, true);
    objectSelected (0);

    if (! writePng (dir.getChildFile ("before-drag.png")))
        return false;
    report << describe ("before-drag", measureLeftRight (0)) << "\n";

    // Mouse-down on object 1, one drag to azimuth 90 at 0.8 of the radius, mouse-up. The events
    // are built the way a real input source builds them.
    const float radius = (float) std::min (map_.getWidth(), map_.getHeight()) * 0.45f;
    const float cx = (float) map_.getWidth() * 0.5f;
    const float cy = (float) map_.getHeight() * 0.5f;
    const float azRad = juce::degreesToRadians (90.0f);
    const juce::Point<float> to { cx - std::sin (azRad) * 0.8f * radius, cy - std::cos (azRad) * 0.8f * radius };
    const auto from = map_.getObjectScreenPos (0);

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

    if (! writePng (dir.getChildFile ("after-drag.png")))
        return false;
    report << describe ("after-drag", measureLeftRight (0)) << "\n";

    return true;
}
