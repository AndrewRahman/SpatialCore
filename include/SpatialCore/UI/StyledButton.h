#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spatialcore
{

/** Centred-text button with no indicator dot — same visual language as IndicatorToggle. */
class StyledButton : public juce::Button
{
public:
    StyledButton(const juce::String& label, const juce::Colour& accentColour,
                 juce::Typeface::Ptr typeface);

    static int getPreferredWidth(juce::Typeface::Ptr typeface, const juce::String& text);

    static constexpr int kHeight = 18;

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    void setLabel(const juce::String& newLabel) { label = newLabel; repaint(); }
    void setAccentColour(const juce::Colour& newAccent) { accent = newAccent; repaint(); }
    void setAlwaysActive(bool active) { alwaysActive = active; repaint(); }
    void setFontSize(float size) { customFontSize = size; repaint(); }
    void setButtonHeight(int h) { customHeight = h; repaint(); }
    void setIcon(const juce::Path& path, float scale, float fixedHeight = 0.0f)
    {
        iconPath = path;
        iconScale = scale;
        fixedIconHeight = fixedHeight;
        repaint();
    }

    int getEffectiveHeight() const { return customHeight > 0 ? customHeight : kHeight; }
    float getEffectiveFontSize() const { return customFontSize > 0.0f ? customFontSize : kFontSize; }

private:
    juce::String label;
    juce::Colour accent;
    juce::Typeface::Ptr typeface;
    bool alwaysActive = false;
    juce::Path iconPath;
    float iconScale = 0.0f;
    float fixedIconHeight = 0.0f;
    float customFontSize = 0.0f;
    int customHeight = 0;

    static constexpr float kFontSize = 10.0f;
    static constexpr float kKerning  = 0.08f;
    static constexpr float kHPad     = 8.0f;
    static constexpr float kCornerR  = 4.0f;
};

} // namespace spatialcore
