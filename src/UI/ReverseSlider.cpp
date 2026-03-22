#include <SpatialCore/UI/ReverseSlider.h>

namespace spatialcore
{

double ReverseSlider::proportionOfLengthToValue(double proportion)
{
    if (reversed)
        proportion = 1.0 - proportion;
    return juce::Slider::proportionOfLengthToValue(proportion);
}

double ReverseSlider::valueToProportionOfLength(double value)
{
    double proportion = juce::Slider::valueToProportionOfLength(value);
    if (reversed)
        proportion = 1.0 - proportion;
    return proportion;
}

void ReverseSlider::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel)
{
    if (reversed)
    {
        auto reversedWheel = wheel;
        reversedWheel.deltaY = -wheel.deltaY;
        juce::Slider::mouseWheelMove(e, reversedWheel);
    }
    else
    {
        juce::Slider::mouseWheelMove(e, wheel);
    }
}

} // namespace spatialcore
