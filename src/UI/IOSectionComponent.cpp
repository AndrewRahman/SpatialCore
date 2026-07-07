#include <SpatialCore/UI/IOSectionComponent.h>

namespace spatialcore
{

//==============================================================================
// IOSectionComponent — generic I/O header-bar container (chrome/layout only).
// Widgets are plain juce::ComboBox/juce::Label with no attachments or
// onChange handlers wired here — the consumer (OSD) binds those after
// construction via the accessors declared in the header.
//==============================================================================
IOSectionComponent::IOSectionComponent (SMLLookAndFeel& lf)
    : lookAndFeel (lf)
{
    inputFormatBox.setLookAndFeel (&lf);
    addAndMakeVisible (inputFormatBox);
    addAndMakeVisible (inputFormatLabel);

    outputFormatBox.setLookAndFeel (&lf);
    addAndMakeVisible (outputFormatBox);
    addAndMakeVisible (outputFormatLabel);

    algorithmBox.setLookAndFeel (&lf);
    addAndMakeVisible (algorithmBox);
    addAndMakeVisible (algorithmLabel);
}

void IOSectionComponent::resized()
{
    // Right-to-left layout within this component's own bounds, mirroring the
    // original inline header-row math (OSD PluginEditor.cpp resized(), the
    // rightmost dropdown group in the header bar): Algorithm/Profile group
    // rightmost, then Output, then Input — each preceded by kGap.
    int rx = getWidth();

    rx -= kAlgoBoxWidth;
    algorithmLabel.setBounds (rx, 0, kAlgoBoxWidth, kLabelHeight);
    algorithmBox.setBounds   (rx, kLabelHeight + 2, kAlgoBoxWidth, kBoxHeight);
    rx -= kGap;

    rx -= kOutputBoxWidth;
    outputFormatLabel.setBounds (rx, 0, kOutputBoxWidth, kLabelHeight);
    outputFormatBox.setBounds   (rx, kLabelHeight + 2, kOutputBoxWidth, kBoxHeight);
    rx -= kGap;

    rx -= kInputBoxWidth;
    inputFormatLabel.setBounds (rx, 0, kInputBoxWidth, kLabelHeight);
    inputFormatBox.setBounds   (rx, kLabelHeight + 2, kInputBoxWidth, kBoxHeight);
}

} // namespace spatialcore
