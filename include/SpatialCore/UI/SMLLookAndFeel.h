#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spatialcore
{

class SMLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Custom typefaces loaded from embedded binary data
    juce::Typeface::Ptr dmSansRegular, dmSansMedium, dmSansBold;
    juce::Typeface::Ptr jetbrainsRegular, jetbrainsMedium, jetbrainsBold;
    juce::Typeface::Ptr robotoMedium;

    SMLLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool isMouseOverButton, bool isButtonDown) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight, int& idealWidth,
                                    int& idealHeight) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                juce::TextEditor& editor) override;

    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;

    juce::Label* createSliderTextBox(juce::Slider& slider) override;

    // SML colour constants
    static constexpr uint32_t kBackground  = 0xFF0A0A14;
    static constexpr uint32_t kCyan        = 0xFF00D4FF;
    static constexpr uint32_t kPurple      = 0xFFA855F7;
    static constexpr uint32_t kAmber       = 0xFFF59E0B;
};

} // namespace spatialcore
