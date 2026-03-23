#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spatialcore
{

class SMLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SMLLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override;

    // SML colour constants
    static constexpr uint32_t kBackground  = 0xFF0A0A14;
    static constexpr uint32_t kCyan        = 0xFF00D4FF;
    static constexpr uint32_t kPurple      = 0xFFA855F7;
    static constexpr uint32_t kAmber       = 0xFFF59E0B;

private:
    juce::Typeface::Ptr dmSansRegular, dmSansMedium, dmSansBold;
    juce::Typeface::Ptr jetbrainsRegular, jetbrainsMedium, jetbrainsBold;
    juce::Typeface::Ptr robotoMedium;
};

} // namespace spatialcore
