#include <SpatialCore/UI/SMLLookAndFeel.h>
#include "SpatialCoreUIFontData.h"

namespace spatialcore
{

//==============================================================================
// SML colour constants (local to this file)
//==============================================================================
namespace SMLColours
{
    static const juce::Colour bgRecessed  (0xff010205);
    static const juce::Colour borderSubtle(0xff252930);
    static const juce::Colour borderDim   (0xff171b20);
    static const juce::Colour textDim     (0xff6d7279);
}

SMLLookAndFeel::SMLLookAndFeel()
{
    // Load embedded fonts
    dmSansRegular    = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::DM_SansRegular_ttf,    SpatialCoreUIFontData::DM_SansRegular_ttfSize);
    dmSansMedium     = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::DM_SansMedium_ttf,     SpatialCoreUIFontData::DM_SansMedium_ttfSize);
    dmSansBold       = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::DM_SansBold_ttf,       SpatialCoreUIFontData::DM_SansBold_ttfSize);
    jetbrainsRegular = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::JetBrains_MonoRegular_ttf, SpatialCoreUIFontData::JetBrains_MonoRegular_ttfSize);
    jetbrainsMedium  = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::JetBrains_MonoMedium_ttf,  SpatialCoreUIFontData::JetBrains_MonoMedium_ttfSize);
    jetbrainsBold    = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::JetBrains_MonoBold_ttf,    SpatialCoreUIFontData::JetBrains_MonoBold_ttfSize);
    robotoMedium     = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::RobotoMedium_ttf,          SpatialCoreUIFontData::RobotoMedium_ttfSize);

    // Popup menu colors
    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff010205));
    setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xff9fa2b0));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff7cc8f0));
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::black);
    setColour(juce::ComboBox::backgroundColourId,             juce::Colour(0xff010205));
    setColour(juce::ComboBox::textColourId,                   juce::Colour(0xff9fa2b0));
    setColour(juce::ComboBox::outlineColourId,                juce::Colour(0xff252930));
    setColour(juce::ComboBox::arrowColourId,                  juce::Colour(0xff6d7080));
}

juce::Typeface::Ptr SMLLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    if (f.getStyleFlags() & juce::Font::bold)
        return dmSansBold;
    return dmSansRegular;
}

juce::Font SMLLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(dmSansRegular).withHeight(13.0f));
}

juce::Font SMLLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(dmSansRegular).withHeight(13.0f));
}

juce::Font SMLLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    // Preset button → DM Sans Regular 13px (match ComboBox font)
    if (button.getComponentID() == "presetButton")
        return juce::Font(juce::FontOptions(dmSansRegular).withHeight(13.0f));
    // Object selector buttons (22px) → JetBrains Mono Medium 12px
    // Other buttons → JetBrains Mono Medium 10px
    float h = (buttonHeight >= 22) ? 12.0f : 10.0f;
    if (jetbrainsMedium)
        return juce::Font(juce::FontOptions(jetbrainsMedium).withHeight(h));
    return juce::Font(juce::FontOptions(h));
}

void SMLLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, const float rotaryStartAngle,
                                       const float rotaryEndAngle, juce::Slider& slider)
{
    float knobDiameter = std::min((float)std::min(width, height), 46.0f);
    auto knobBounds = juce::Rectangle<float>(x + (width - knobDiameter) * 0.5f,
                                              y + (height - knobDiameter) * 0.5f,
                                              knobDiameter, knobDiameter);

    auto cx = knobBounds.getCentreX();
    auto cy = knobBounds.getCentreY();
    auto arcR = knobDiameter * 0.5f - 5.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 2.2f;

    // Dark circle background
    g.setColour(SMLColours::bgRecessed);
    g.fillEllipse(cx - arcR - 1.5f, cy - arcR - 1.5f, (arcR + 1.5f) * 2.0f, (arcR + 1.5f) * 2.0f);
    g.setColour(SMLColours::borderDim);
    g.drawEllipse(cx - arcR - 1.5f, cy - arcR - 1.5f, (arcR + 1.5f) * 2.0f, (arcR + 1.5f) * 2.0f, 0.8f);

    // Background track arc
    juce::Path backgroundTrack;
    backgroundTrack.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(SMLColours::borderSubtle);
    g.strokePath(backgroundTrack, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc with glow
    if (slider.isEnabled())
    {
        auto accentColour = slider.findColour(juce::Slider::thumbColourId);
        if (accentColour == juce::Colours::transparentBlack)
            accentColour = juce::Colour(0xff7cc8f0);

        juce::Path valueArc;
        valueArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(accentColour.withAlpha(0.3f));
        g.strokePath(valueArc, juce::PathStrokeType(lineW + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(accentColour);
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Tick marks
    g.setColour(SMLColours::borderSubtle);
    for (float t : { 0.0f, 0.5f, 1.0f })
    {
        float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        float cosA = std::cos(tickAngle - juce::MathConstants<float>::halfPi);
        float sinA = std::sin(tickAngle - juce::MathConstants<float>::halfPi);
        float inner = arcR + 2.0f;
        float outer = arcR + 5.0f;
        g.drawLine(cx + cosA * inner, cy + sinA * inner,
                    cx + cosA * outer, cy + sinA * outer, 0.7f);
    }

    // Center dot
    g.setColour(juce::Colour(0xff3e4150));
    g.fillEllipse(cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);

    // Indicator needle
    if (slider.isEnabled())
    {
        auto accentColour = slider.findColour(juce::Slider::thumbColourId);
        if (accentColour == juce::Colours::transparentBlack)
            accentColour = juce::Colour(0xff7cc8f0);

        float cosA = std::cos(angle - juce::MathConstants<float>::halfPi);
        float sinA = std::sin(angle - juce::MathConstants<float>::halfPi);
        float innerR = arcR - 4.0f;
        float outerR = arcR + 1.0f;
        g.setColour(accentColour.withAlpha(0.85f));
        g.drawLine(cx + cosA * innerR, cy + sinA * innerR,
                    cx + cosA * outerR, cy + sinA * outerR, 1.3f);
    }
}

void SMLLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& backgroundColour,
                                           bool isMouseOverButton, bool isButtonDown)
{
    if (backgroundColour.isTransparent())
        return;

    if (button.getComponentID() == "presetButton")
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)button.getWidth(), (float)button.getHeight());
        auto baseColour = backgroundColour;
        if (isButtonDown)            baseColour = baseColour.brighter(0.15f);
        else if (isMouseOverButton)  baseColour = baseColour.brighter(0.12f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(SMLColours::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        float arrowX = (float)button.getWidth() - 12.0f;
        float arrowY = (float)button.getHeight() * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(arrowX - 3.0f, arrowY - 1.5f,
                           arrowX + 3.0f, arrowY - 1.5f,
                           arrowX,        arrowY + 2.5f);
        g.setColour(SMLColours::textDim);
        g.fillPath(arrow);
        return;
    }

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = backgroundColour;
    if (isButtonDown)            baseColour = baseColour.brighter(0.15f);
    else if (isMouseOverButton)  baseColour = baseColour.brighter(0.12f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);

    float borderBright = isMouseOverButton ? 0.5f : 0.3f;
    auto borderCol = baseColour.brighter(borderBright).withAlpha(isMouseOverButton ? 0.8f : 0.6f);
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void SMLLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                     bool isMouseOverButton, bool isButtonDown)
{
    if (button.getComponentID() == "presetButton")
    {
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        g.drawText(button.getButtonText(),
                    6, 0, button.getWidth() - 20, button.getHeight(),
                    juce::Justification::centredLeft, true);
        return;
    }

    LookAndFeel_V4::drawButtonText(g, button, isMouseOverButton, isButtonDown);
}

void SMLLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(juce::Colour(0xff252930));
    g.drawRect(0, 0, width, height, 1);
}

void SMLLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                        bool isSeparator, bool isActive, bool isHighlighted,
                                        bool isTicked, bool hasSubMenu,
                                        const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                        const juce::Drawable* /*icon*/, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto sepArea = area.reduced(8, 0);
        g.setColour(juce::Colour(0xff252930));
        g.fillRect(sepArea.getX(), area.getCentreY(), sepArea.getWidth(), 1);
        return;
    }

    auto textBounds = area.reduced(8, 0);

    if (isHighlighted && isActive)
    {
        g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRect(area);
        g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
    }
    else
    {
        g.setColour(textColour != nullptr ? *textColour
                     : (isActive ? findColour(juce::PopupMenu::textColourId)
                                 : findColour(juce::PopupMenu::textColourId).withAlpha(0.4f)));
    }

    g.setFont(getPopupMenuFont());

    if (isTicked)
    {
        auto tickBounds = area.withWidth(20);
        g.drawText(juce::String::charToString(0x2713), tickBounds,
                    juce::Justification::centred);
        textBounds = textBounds.withTrimmedLeft(14);
    }

    g.drawFittedText(text, textBounds, juce::Justification::centredLeft, 1);

    if (hasSubMenu)
    {
        float arrowX = (float)(area.getRight() - 12);
        float arrowY = (float)area.getCentreY();
        juce::Path arrow;
        arrow.addTriangle(arrowX - 1.5f, arrowY - 3.0f,
                           arrowX - 1.5f, arrowY + 3.0f,
                           arrowX + 2.5f, arrowY);
        g.fillPath(arrow);
    }
}

void SMLLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                int /*standardMenuItemHeight*/,
                                                int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 50;
        idealHeight = 8;
        return;
    }

    auto font = getPopupMenuFont();
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, text, 0.0f, 0.0f);
    idealWidth = static_cast<int>(std::ceil(ga.getBoundingBox(0, -1, true).getWidth())) + 32;
    idealHeight = 24;
}

void SMLLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                   int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                   juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);

    g.setColour(findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    if (box.isEnabled())
    {
        float arrowX = (float)width - 12.0f;
        float arrowY = (float)height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(arrowX - 3.0f, arrowY - 1.5f,
                           arrowX + 3.0f, arrowY - 1.5f,
                           arrowX,        arrowY + 2.5f);
        g.setColour(findColour(juce::ComboBox::arrowColourId));
        g.fillPath(arrow);
    }
}

void SMLLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    if (box.isEnabled())
        label.setBounds(1, 1, box.getWidth() - 20, box.getHeight() - 2);
    else
        label.setBounds(0, 1, box.getWidth(), box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

void SMLLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr
        && jetbrainsRegular)
    {
        g.setFont(juce::Font(juce::FontOptions(jetbrainsRegular).withHeight(12.0f)));
        g.setColour(label.findColour(juce::Label::textColourId));
        g.drawText(label.getText(), label.getLocalBounds(),
                    label.getJustificationType(), true);
        return;
    }

    auto bounds = label.getLocalBounds().toFloat();
    auto bg = label.findColour(juce::Label::backgroundColourId);
    if (! bg.isTransparent())
    {
        g.setColour(bg);
        g.fillRect(bounds);
    }
    auto outline = label.findColour(juce::Label::outlineColourId);
    if (! outline.isTransparent())
    {
        g.setColour(outline);
        g.drawRect(bounds.reduced(0.5f), 1.0f);
    }
    if (! label.isBeingEdited())
    {
        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(label.getFont());
        g.drawFittedText(label.getText(), textArea,
                          label.getJustificationType(),
                          juce::jmax(1, (int)((float)textArea.getHeight() / label.getFont().getHeight())),
                          label.getMinimumHorizontalScale());
    }
}

void SMLLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int w, int h,
                                            juce::TextEditor& editor)
{
    auto colour = editor.hasKeyboardFocus(true)
                    ? editor.findColour(juce::TextEditor::focusedOutlineColourId)
                    : editor.findColour(juce::TextEditor::outlineColourId);
    if (colour.isTransparent())
        return;
    g.setColour(colour);

    auto* parent = editor.getParentComponent();
    bool isKnobEditor = parent != nullptr
                        && dynamic_cast<juce::Slider*>(parent->getParentComponent()) != nullptr;

    if (isKnobEditor)
        g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(w) - 1.0f,
                                static_cast<float>(h) - 1.0f, 3.0f, 1.0f);
    else
        g.drawRect(0.5f, 0.5f, static_cast<float>(w) - 1.0f,
                    static_cast<float>(h) - 1.0f, 1.0f);
}

juce::Label* SMLLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = new juce::Label();
    label->setJustificationType(juce::Justification::centred);
    label->setKeyboardType(juce::TextInputTarget::decimalKeyboard);
    label->setColour(juce::Label::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    label->setColour(juce::Label::backgroundColourId,
                      (slider.getSliderStyle() == juce::Slider::LinearBar
                       || slider.getSliderStyle() == juce::Slider::LinearBarVertical)
                          ? juce::Colours::transparentBlack
                          : slider.findColour(juce::Slider::textBoxBackgroundColourId));
    label->setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    if (jetbrainsRegular)
        label->setFont(juce::Font(juce::FontOptions(jetbrainsRegular).withHeight(11.0f)));
    else
        label->setFont(juce::Font(juce::FontOptions(11.0f)));
    label->setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
    label->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff0A0A14));
    label->setColour(juce::TextEditor::highlightColourId,
                      slider.findColour(juce::Slider::thumbColourId).withAlpha(0.35f));
    label->setColour(juce::Label::outlineWhenEditingColourId,
                      slider.findColour(juce::Slider::thumbColourId));
    label->onEditorShow = [label]()
    {
        if (auto* ed = label->getCurrentTextEditor())
        {
            ed->setJustification(juce::Justification::centred);
            ed->setHighlightedRegion({ 0, label->getText().length() });
        }
    };
    return label;
}

} // namespace spatialcore
