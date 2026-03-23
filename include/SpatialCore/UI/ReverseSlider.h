#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spatialcore
{

class ReverseSlider : public juce::Slider
{
public:
    void setReversed(bool r) { reversed = r; }
    bool isReversed() const { return reversed; }

    double proportionOfLengthToValue(double proportion) override;
    double valueToProportionOfLength(double value) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

private:
    bool reversed = false;
};

} // namespace spatialcore
