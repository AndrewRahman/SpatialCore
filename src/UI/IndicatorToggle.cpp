#include <SpatialCore/UI/IndicatorToggle.h>

namespace spatialcore
{

IndicatorToggle::IndicatorToggle(const juce::String& label, const juce::Colour& accentColour,
                                  juce::Typeface::Ptr typeface)
    : juce::Button(label), accent(accentColour), face(typeface)
{
    setClickingTogglesState(true);
}

int IndicatorToggle::getPreferredWidth(juce::Typeface::Ptr /*typeface*/, const juce::String& /*text*/)
{
    return 60; // Stub — real measurement during extraction
}

void IndicatorToggle::paintButton(juce::Graphics& /*g*/, bool /*isMouseOverButton*/, bool /*isButtonDown*/)
{
    // Stub — real indicator toggle painting during extraction
}

void IndicatorToggle::setAccentColour(const juce::Colour& newAccent)
{
    accent = newAccent;
    repaint();
}

} // namespace spatialcore
