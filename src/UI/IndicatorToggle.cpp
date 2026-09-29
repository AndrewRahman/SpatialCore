#include <SpatialCore/UI/IndicatorToggle.h>

namespace spatialcore
{

namespace
{
    // Colours matching OSD's Colours_OSD namespace (Source/PluginEditor.cpp)
    const juce::Colour bgRecessed  (0xff010205);
    const juce::Colour borderSubtle(0xff252930);
    const juce::Colour textDim     (0xff6d7279);

    // Helper: create a Font from a specific Typeface with height and optional kerning
    juce::Font makeFont (juce::Typeface::Ptr tf, float height, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (tf).withHeight (height).withKerningFactor (kerning));
    }

    // Shared background + border painting for toggle-style buttons
    void paintToggleButtonBg (juce::Graphics& g, juce::Rectangle<float> bounds,
                               const juce::Colour& accent, bool isOn, bool isHover, float cornerR)
    {
        float bgAlpha = isOn ? 0.08f : 0.0f;
        if (isHover) bgAlpha += 0.06f;
        g.setColour (isOn ? accent.withAlpha (bgAlpha)
                          : (isHover ? accent.withAlpha (0.04f) : bgRecessed));
        g.fillRoundedRectangle (bounds, cornerR);

        float borderAlpha = isOn ? 0.6f : 0.0f;
        if (isHover && !isOn)  borderAlpha = 0.3f;
        else if (isHover && isOn) borderAlpha = 0.85f;
        g.setColour (isOn || isHover ? accent.withAlpha (borderAlpha) : borderSubtle);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerR, 1.0f);
    }
}

IndicatorToggle::IndicatorToggle (const juce::String& lbl, const juce::Colour& col,
                                   juce::Typeface::Ptr tf)
    : juce::Button (lbl), label (lbl), accent (col), typeface (tf)
{
    setClickingTogglesState (true);
}

int IndicatorToggle::getPreferredWidth (juce::Typeface::Ptr tf, const juce::String& text)
{
    auto font = makeFont (tf, kFontSize, kKerning);
    juce::GlyphArrangement gl;
    gl.addLineOfText (font, text, 0.0f, 0.0f);
    return (int) kLeftPad + (int) (kDotRadius * 2.0f) + (int) kDotGap
         + juce::roundToInt (gl.getBoundingBox (0, gl.getNumGlyphs(), true).getWidth())
         + (int) kRightPad;
}

void IndicatorToggle::paintButton (juce::Graphics& g, bool isMouseOverButton, bool /*isButtonDown*/)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0.0f) return;

    bool isOn = getToggleState();
    paintToggleButtonBg (g, bounds, accent, isOn, isMouseOverButton, kCornerR);

    // Indicator dot
    float dotX = bounds.getX() + kLeftPad + kDotRadius;
    float dotY = bounds.getCentreY();
    g.setColour (isOn ? accent
                      : (isMouseOverButton ? textDim.withAlpha (0.6f)
                                           : textDim.withAlpha (0.4f)));
    g.fillEllipse (dotX - kDotRadius, dotY - kDotRadius, kDotRadius * 2.0f, kDotRadius * 2.0f);
    if (isOn)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (dotX - kDotRadius - 2.0f, dotY - kDotRadius - 2.0f,
                       (kDotRadius + 2.0f) * 2.0f, (kDotRadius + 2.0f) * 2.0f);
    }

    // Text label
    g.setColour (isOn ? accent
                      : (isMouseOverButton ? juce::Colour (0xff9fa5ae) : textDim));
    g.setFont (makeFont (typeface, kFontSize, kKerning));
    auto textBounds = bounds.withLeft (dotX + kDotRadius + kDotGap);
    g.drawText (label, textBounds.toNearestInt(), juce::Justification::centredLeft);
}

void IndicatorToggle::setAccentColour (const juce::Colour& newAccent)
{
    accent = newAccent;
    repaint();
}

} // namespace spatialcore
