#include <SpatialCore/UI/SMLLookAndFeel.h>

namespace spatialcore
{

SMLLookAndFeel::SMLLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(kBackground));
}

juce::Typeface::Ptr SMLLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    // Stub — real font loading from BinaryData during extraction
    return juce::LookAndFeel_V4::getTypefaceForFont(f);
}

} // namespace spatialcore
