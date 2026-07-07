#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spatialcore
{

class IndicatorToggle : public juce::Button
{
public:
    IndicatorToggle(const juce::String& label, const juce::Colour& accentColour,
                    juce::Typeface::Ptr typeface);

    static int getPreferredWidth(juce::Typeface::Ptr typeface, const juce::String& text);

    static constexpr int kHeight = 18;
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    /** Update accent colour dynamically (e.g. tap-color-aware ON button). */
    void setAccentColour(const juce::Colour& newAccent);

private:
    juce::String label;
    juce::Colour accent;
    juce::Typeface::Ptr typeface;

    static constexpr float kFontSize  = 10.0f;
    static constexpr float kKerning   = 0.08f;
    static constexpr float kDotRadius = 2.5f;
    static constexpr float kLeftPad   = 6.0f;
    static constexpr float kDotGap    = 4.0f;
    static constexpr float kRightPad  = 7.0f;
    static constexpr float kCornerR   = 4.0f;
};

} // namespace spatialcore
