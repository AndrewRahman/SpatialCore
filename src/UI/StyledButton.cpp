#include <SpatialCore/UI/StyledButton.h>

namespace spatialcore
{

StyledButton::StyledButton(const juce::String& label, const juce::Colour& accentColour,
                            juce::Typeface::Ptr typeface)
    : juce::Button(label), accent(accentColour), face(typeface), labelText(label)
{
}

int StyledButton::getPreferredWidth(juce::Typeface::Ptr /*typeface*/, const juce::String& /*text*/)
{
    return 60; // Stub
}

void StyledButton::paintButton(juce::Graphics& /*g*/, bool /*isMouseOverButton*/, bool /*isButtonDown*/)
{
    // Stub — real styled button painting during extraction
}

void StyledButton::setLabel(const juce::String& newLabel) { labelText = newLabel; repaint(); }
void StyledButton::setAccentColour(const juce::Colour& newAccent) { accent = newAccent; repaint(); }
void StyledButton::setAlwaysActive(bool active) { alwaysActive = active; repaint(); }
void StyledButton::setFontSize(float size) { fontSize = size; repaint(); }
void StyledButton::setButtonHeight(int h) { buttonHeight = h; repaint(); }

void StyledButton::setIcon(const juce::Path& path, float scale, float fixedHeight)
{
    iconPath = path;
    iconScale = scale;
    iconFixedHeight = fixedHeight;
    repaint();
}

int   StyledButton::getEffectiveHeight() const { return buttonHeight; }
float StyledButton::getEffectiveFontSize() const { return fontSize; }

} // namespace spatialcore
