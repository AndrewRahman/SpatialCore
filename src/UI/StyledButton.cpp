#include <SpatialCore/UI/StyledButton.h>

namespace spatialcore
{

namespace
{
    // Colours matching OSD's Colours_OSD namespace (Source/PluginEditor.cpp)
    const juce::Colour bgRecessed    (0xff010205);
    const juce::Colour borderSubtle  (0xff252930);
    const juce::Colour textSecondary (0xff9fa5ae);
    const juce::Colour textDim       (0xff6d7279);

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

StyledButton::StyledButton (const juce::String& lbl, const juce::Colour& col,
                            juce::Typeface::Ptr tf)
    : juce::Button (lbl), label (lbl), accent (col), typeface (tf)
{
    setClickingTogglesState (true);
}

int StyledButton::getPreferredWidth (juce::Typeface::Ptr tf, const juce::String& text)
{
    auto font = makeFont (tf, kFontSize, kKerning);
    juce::GlyphArrangement gl;
    gl.addLineOfText (font, text, 0.0f, 0.0f);
    return (int) kHPad
         + juce::roundToInt (gl.getBoundingBox (0, gl.getNumGlyphs(), true).getWidth())
         + (int) kHPad;
}

void StyledButton::paintButton (juce::Graphics& g, bool isMouseOverButton, bool /*isButtonDown*/)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0.0f) return;

    bool isOn = alwaysActive || getToggleState();

    paintToggleButtonBg (g, bounds, accent, isOn, isMouseOverButton, kCornerR);

    // Text (+ optional icon) — centred
    float fontSize = getEffectiveFontSize();
    auto textCol = isOn ? accent
                        : (isMouseOverButton ? textSecondary : textDim);
    g.setColour (textCol);
    g.setFont (makeFont (typeface, fontSize, kKerning));

    if (iconScale > 0.0f)
    {
        auto pathBounds = iconPath.getBounds();
        float iconH = (fixedIconHeight > 0.0f) ? fixedIconHeight : (bounds.getHeight() - 6.0f);
        float s = iconH / pathBounds.getHeight();
        float iconW = pathBounds.getWidth() * s;

        if (label.isEmpty())
        {
            // Icon-only: centre the icon
            float startX = bounds.getCentreX() - iconW * 0.5f;
            g.fillPath (iconPath,
                        juce::AffineTransform::translation (-pathBounds.getX(), -pathBounds.getY())
                            .scaled (s)
                            .translated (startX, bounds.getCentreY() - iconH * 0.5f));
        }
        else
        {
            // Icon + text: compute combined width, centre both
            constexpr float gap = 3.0f;
            juce::GlyphArrangement gl;
            auto font = makeFont (typeface, fontSize, kKerning);
            gl.addLineOfText (font, label, 0.0f, 0.0f);
            float textW = gl.getBoundingBox (0, gl.getNumGlyphs(), true).getWidth();
            float totalW = iconW + gap + textW;
            float startX = bounds.getCentreX() - totalW * 0.5f;

            g.fillPath (iconPath,
                        juce::AffineTransform::translation (-pathBounds.getX(), -pathBounds.getY())
                            .scaled (s)
                            .translated (startX, bounds.getCentreY() - iconH * 0.5f));

            auto textRect = juce::Rectangle<float> (startX + iconW + gap, bounds.getY(),
                                                     textW + 2.0f, bounds.getHeight());
            g.drawText (label, textRect.toNearestInt(), juce::Justification::centredLeft);
        }
    }
    else
    {
        g.drawText (label, bounds.toNearestInt(), juce::Justification::centred);
    }
}

} // namespace spatialcore
