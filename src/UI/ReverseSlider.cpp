#include <SpatialCore/UI/ReverseSlider.h>

namespace spatialcore
{

double ReverseSlider::proportionOfLengthToValue(double proportion)
{
    if (reversed)
        return juce::Slider::proportionOfLengthToValue(1.0 - proportion);
    return juce::Slider::proportionOfLengthToValue(proportion);
}

double ReverseSlider::valueToProportionOfLength(double value)
{
    double proportion = juce::Slider::valueToProportionOfLength(value);
    if (reversed)
        return 1.0 - proportion;
    return proportion;
}

void ReverseSlider::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel)
{
    if (reversed)
    {
        auto rw = wheel;
        rw.deltaX = -wheel.deltaX;
        rw.deltaY = -wheel.deltaY;
        juce::Slider::mouseWheelMove(e, rw);
    }
    else
    {
        juce::Slider::mouseWheelMove(e, wheel);
    }
}

} // namespace spatialcore
