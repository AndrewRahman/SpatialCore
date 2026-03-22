#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spatialcore
{

class StyledButton : public juce::Button
{
public:
    StyledButton(const juce::String& label, const juce::Colour& accentColour,
                 juce::Typeface::Ptr typeface);

    static int getPreferredWidth(juce::Typeface::Ptr typeface, const juce::String& text);

    static constexpr int kHeight = 18;
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    void setLabel(const juce::String& newLabel);
    void setAccentColour(const juce::Colour& newAccent);
    void setAlwaysActive(bool active);
    void setFontSize(float size);
    void setButtonHeight(int h);
    void setIcon(const juce::Path& path, float scale, float fixedHeight = 0.0f);

    int   getEffectiveHeight() const;
    float getEffectiveFontSize() const;

private:
    juce::Colour accent;
    juce::Typeface::Ptr face;
    juce::String labelText;
    bool alwaysActive = false;
    float fontSize = 10.0f;
    int buttonHeight = kHeight;
    juce::Path iconPath;
    float iconScale = 1.0f;
    float iconFixedHeight = 0.0f;

    static constexpr float kKerning = 0.08f;
    static constexpr float kHPad    = 8.0f;
    static constexpr float kCornerR = 4.0f;
};

} // namespace spatialcore
