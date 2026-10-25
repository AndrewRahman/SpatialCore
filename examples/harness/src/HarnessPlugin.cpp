#include "HarnessPlugin.h"

#include <algorithm>
#include <cmath>

using spatialcore::OutputFormat;
using spatialcore::OutputFormatRegistry;
using spatialcore::RenderBlockContext;
using spatialcore::RenderSources;

namespace
{
// Where the two objects start: 30 degrees either side of straight ahead, at ear level, at the
// edge of the map's distance range.
constexpr float kStartAzimuth[HarnessProcessor::kNumObjects] = { 30.0f, -30.0f };
} // namespace

HarnessProcessor::HarnessProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < kNumObjects; ++i)
    {
        const auto idx = static_cast<size_t> (i);
        objectAzimuth_[idx].store (kStartAzimuth[i]);
        objectElevation_[idx].store (0.0f);
        objectDistance_[idx].store (1.0f);
    }

    engine_.setOutputFormat (OutputFormat::Binaural);

    // Ask for profile 5 (MIT KEMAR, the one profile that is always embedded). The request is held
    // until the first prepare(), then served on the engine's own loader thread (DATA-01).
    engine_.setHRTFProfile (hrtfProfile_);
}

void HarnessProcessor::setOutputFormat (OutputFormat format)
{
    formatIndex_ = static_cast<int> (format);
    // Message thread, allocating (it builds the speaker layout). The engine publishes the layout
    // through a wait-free three-slot handoff, so the next block renders the last call.
    engine_.setOutputFormat (format);
}

void HarnessProcessor::setAlgorithmIndex (int index)
{
    algorithmIndex_ = index;
    // Lock-free (one relaxed atomic store, clamped); the engine reads it once per block.
    engine_.setAlgorithmIndex (index);
}

void HarnessProcessor::setHRTFProfile (int profileIndex)
{
    hrtfProfile_ = profileIndex;
    // Message thread, returns at once: the engine's worker loads the profile off the audio thread
    // and the audio thread crossfades to it. 0 is the Simple profile, 1-5 the SOFA profiles.
    engine_.setHRTFProfile (profileIndex);
}

void HarnessProcessor::setObjectPosition (int object, float azimuthDeg, float elevationDeg, float distance)
{
    if (object < 0 || object >= kNumObjects)
        return;

    const auto idx = static_cast<size_t> (object);
    objectAzimuth_[idx].store (azimuthDeg, std::memory_order_relaxed);
    objectElevation_[idx].store (elevationDeg, std::memory_order_relaxed);
    objectDistance_[idx].store (distance, std::memory_order_relaxed);
}

void HarnessProcessor::getObjectPosition (int object, float& azimuthDeg, float& elevationDeg, float& distance) const noexcept
{
    const auto idx = static_cast<size_t> (juce::jlimit (0, kNumObjects - 1, object));
    azimuthDeg = objectAzimuth_[idx].load (std::memory_order_relaxed);
    elevationDeg = objectElevation_[idx].load (std::memory_order_relaxed);
    distance = objectDistance_[idx].load (std::memory_order_relaxed);
}

void HarnessProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedSampleRate_ = sampleRate;
    preparedBlockSize_ = std::max (1, samplesPerBlock);

    engine_.prepare (sampleRate, preparedBlockSize_);

    // Size every scratch buffer here, never in processBlock.
    const auto n = static_cast<size_t> (preparedBlockSize_);
    for (auto& b : monoBuffers_)
        b.assign (n, 0.0f);
    for (auto& b : distanceGains_)
        b.assign (n, 1.0f);
    tapFade_.assign (n, 1.0f);   // every object stays enabled, so the tap fade is a constant 1
    for (auto& b : renderBank_)
        b.assign (n, 0.0f);
}

bool HarnessProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    const bool inputOk = in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const int outChannels = out.size();
    return inputOk && outChannels >= 1 && outChannels <= kMaxChannels;
}

void HarnessProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalSamples = buffer.getNumSamples();
    const int numHostChannels = buffer.getNumChannels();
    const bool hasInput = getTotalNumInputChannels() > 0 && numHostChannels > 0;

    if (preparedBlockSize_ <= 0)   // processBlock before prepareToPlay: nothing to render with
    {
        buffer.clear();
        return;
    }

    float* bankPointers[kMaxChannels];
    for (int c = 0; c < kMaxChannels; ++c)
        bankPointers[c] = renderBank_[static_cast<size_t> (c)].data();

    // A host block longer than the prepared size is rendered in pieces of at most the prepared
    // size, so nothing here ever has to grow.
    for (int offset = 0; offset < totalSamples; offset += preparedBlockSize_)
    {
        const int n = std::min (preparedBlockSize_, totalSamples - offset);
        const auto count = static_cast<size_t> (n);

        // The same mono input feeds every object; the position is what differs. The host buffer is
        // read before this piece of it is overwritten with output, so reading channel 0 is safe.
        RenderSources sources;
        sources.numSamples = n;

        for (int t = 0; t < kNumObjects; ++t)
        {
            const auto idx = static_cast<size_t> (t);
            float* mono = monoBuffers_[idx].data();
            if (hasInput)
                std::copy_n (buffer.getReadPointer (0) + offset, count, mono);
            else
                std::fill_n (mono, count, 0.0f);

            spatialcore::ObjectState state;
            state.azimuthDeg = objectAzimuth_[idx].load (std::memory_order_relaxed);
            state.elevationDeg = objectElevation_[idx].load (std::memory_order_relaxed);
            state.distance = objectDistance_[idx].load (std::memory_order_relaxed);
            state.enabled = true;

            std::fill_n (distanceGains_[idx].data(), count, spatialcore::distanceAttenuation (state.distance));

            sources.monoBuffers[t] = mono;
            sources.tapFadeGainPerSample[t] = tapFade_.data();
            sources.distGainPerSample[t] = distanceGains_[idx].data();
            sources.objectLive[t] = true;
            sources.sourceEnabledBlock[t] = true;
            sources.objects[t] = state;
        }

        for (auto& channel : renderBank_)
            std::fill_n (channel.data(), count, 0.0f);

        // All four opt-in flags, set on every block. Each one removes a piece of consumer glue and
        // each default is false, so a consumer that sets none renders exactly as before.
        RenderBlockContext ctx;
        ctx.sampleRate = preparedSampleRate_;
        // The engine computes objGains / objChannelGains (and, on a stereo variant, objGainL / objGainR
        // and stereoMode) from the positions and the algorithm chosen with setAlgorithmIndex.
        ctx.engineComputesGains = true;
        // The engine derives activeFormat, isStereoVariant, isBinaural, isAmbiOutput and ambiOrder
        // from the layout it renders against, so a format switch can never tear against the dispatch.
        ctx.engineDerivesDispatch = true;
        // The engine picks the binaural path from its own active renderer and blends Simple and HRTF
        // during a profile switch, so profile 0 fades like any other.
        ctx.engineSelectsHRTF = true;
        // An output-format switch fades out and back in (a two-block dip) instead of clicking.
        ctx.engineFadesFormatSwitch = true;

        engine_.renderBlock (sources, ctx, bankPointers, kMaxChannels);

        // The channel map comes from the layout the block RENDERED with, read by reference after
        // renderBlock (a LayoutState holds a std::vector, so never copy it on the audio thread).
        // getActiveLayout() would be the writer thread's view and can be one switch ahead.
        const OutputFormat rendered = engine_.getBlockLayout().format;
        const int required = OutputFormatRegistry::getInfo (rendered).requiredChannels;
        const int toCopy = std::min ({ required, numHostChannels, kMaxChannels });

        for (int c = 0; c < toCopy; ++c)
            std::copy_n (renderBank_[static_cast<size_t> (c)].data(), count, buffer.getWritePointer (c) + offset);
        for (int c = toCopy; c < numHostChannels; ++c)
            std::fill_n (buffer.getWritePointer (c) + offset, count, 0.0f);

        lastRenderedFormat_.store (static_cast<int> (rendered), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* HarnessProcessor::createEditor()
{
    return new HarnessEditor (*this);
}

void HarnessProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("HarnessState");
    state.setProperty ("format", formatIndex_, nullptr);
    state.setProperty ("algorithm", algorithmIndex_, nullptr);
    state.setProperty ("hrtfProfile", hrtfProfile_, nullptr);

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void HarnessProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
    if (! state.isValid() || ! state.hasType ("HarnessState"))
        return;

    // Profile indices and algorithm indices are persisted by consumers and never renumbered, so
    // the saved numbers are passed straight back in. The engine clamps or rejects unknown ones.
    const int format = juce::jlimit (0, spatialcore::NUM_OUTPUT_FORMATS - 1,
                                     static_cast<int> (state.getProperty ("format", formatIndex_)));
    setOutputFormat (static_cast<OutputFormat> (format));
    setAlgorithmIndex (static_cast<int> (state.getProperty ("algorithm", algorithmIndex_)));
    setHRTFProfile (static_cast<int> (state.getProperty ("hrtfProfile", hrtfProfile_)));
}

//==============================================================================
HarnessEditor::HarnessEditor (HarnessProcessor& owner)
    : juce::AudioProcessorEditor (owner), processor_ (owner)
{
    setLookAndFeel (&lookAndFeel_.get());
    addAndMakeVisible (map_);
    setSize (420, 420);
    startTimerHz (30);
}

HarnessEditor::~HarnessEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);   // the shared look-and-feel outlives this editor; never leave a dangling pointer
}

void HarnessEditor::resized()
{
    map_.setBounds (getLocalBounds());
}

void HarnessEditor::timerCallback()
{
    for (int i = 0; i < HarnessProcessor::kNumObjects; ++i)
    {
        float az = 0.0f, el = 0.0f, dist = 0.0f;
        processor_.getObjectPosition (i, az, el, dist);
        map_.setObjectState (i, az, el, dist, true);
    }
}

//==============================================================================
// Declared by juce_audio_plugin_client in the VST3 build; declared here too so the console check,
// which does not link the plugin client, compiles this file without a missing-prototype warning.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HarnessProcessor();
}
