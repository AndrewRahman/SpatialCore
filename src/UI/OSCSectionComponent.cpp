#include <SpatialCore/UI/OSCSectionComponent.h>

namespace spatialcore
{

//==============================================================================
// OSCSectionComponent — generic OSC receive/send row container (chrome/layout
// + toggle widgets only). Connection state, port/IP validation, and
// onClick/onTextChange handlers are wired by the consumer (OSD) after
// construction via the accessors declared in the header.
//==============================================================================
OSCSectionComponent::OSCSectionComponent (const juce::String& receiveLabel, const juce::String& sendLabel,
                                         const juce::Colour& accentColour, juce::Typeface::Ptr typeface)
    : toggleTypeface (typeface),
      receiveToggle (receiveLabel, accentColour, typeface),
      sendToggle (sendLabel, accentColour, typeface)
{
    addAndMakeVisible (receiveToggle);
    addAndMakeVisible (receivePortLabel);
    addAndMakeVisible (sendToggle);
    addAndMakeVisible (sendIPLabel);
    addAndMakeVisible (sendPortLabel);
}

void OSCSectionComponent::paint (juce::Graphics& g)
{
    // "Port" label for the Receive row.
    {
        auto bounds = receiveToggle.getBounds().toFloat();
        if (bounds.getWidth() > 0)
        {
            g.setColour (labelColour);
            g.setFont (labelTypeface != nullptr
                           ? juce::Font (juce::FontOptions (labelTypeface).withHeight (10.0f).withKerningFactor (0.08f))
                           : juce::Font (juce::FontOptions (10.0f)));
            g.drawText ("Port", (int) (bounds.getRight() + 4), (int) bounds.getY(),
                        28, (int) bounds.getHeight(), juce::Justification::centredLeft);
        }
    }

    // "IP" and "Port" labels for the Send row.
    {
        auto sendBounds = sendToggle.getBounds().toFloat();
        if (sendBounds.getWidth() > 0)
        {
            g.setColour (labelColour);
            g.setFont (labelTypeface != nullptr
                           ? juce::Font (juce::FontOptions (labelTypeface).withHeight (10.0f).withKerningFactor (0.08f))
                           : juce::Font (juce::FontOptions (10.0f)));
            g.drawText ("IP", (int) (sendBounds.getRight() + 4), (int) sendBounds.getY(),
                        14, (int) sendBounds.getHeight(), juce::Justification::centredLeft);
            auto ipBounds = sendIPLabel.getBounds().toFloat();
            g.drawText ("Port", (int) (ipBounds.getRight() + 4), (int) ipBounds.getY(),
                        28, (int) ipBounds.getHeight(), juce::Justification::centredLeft);
        }
    }
}

void OSCSectionComponent::resized()
{
    // Mirrors the original inline OSC section layout (OSD PluginEditor.cpp
    // resized(), right-panel OSC rows), relative to this component's own
    // origin: Receive row at local y=0, Send row at local y=kRowGap.
    int rcvW = IndicatorToggle::getPreferredWidth (toggleTypeface, receiveToggle.getButtonText());
    int sndW = IndicatorToggle::getPreferredWidth (toggleTypeface, sendToggle.getButtonText());

    // Receive row
    receiveToggle.setBounds (0, 0, rcvW, kRowHeight);
    receivePortLabel.setBounds (rcvW + kPortGap, 0, kPortLabelW, kRowHeight);

    // Send row
    int sendY = kRowGap;
    sendToggle.setBounds (0, sendY, sndW, kRowHeight);
    sendIPLabel.setBounds (sndW + kSendFieldGap, sendY, kIPLabelW, kRowHeight);
    sendPortLabel.setBounds (sndW + kSendFieldGap + kIPLabelW + kSendFieldGap, sendY, kSendPortW, kRowHeight);
}

} // namespace spatialcore
