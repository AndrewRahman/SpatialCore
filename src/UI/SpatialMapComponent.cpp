#include <SpatialCore/UI/SpatialMapComponent.h>

namespace spatialcore
{

const juce::Colour SpatialMapComponent::objectColours[MAX_SOURCES] = {
    juce::Colour(0xFF00D4FF), // Cyan
    juce::Colour(0xFFA855F7), // Purple
    juce::Colour(0xFFF59E0B), // Amber
    juce::Colour(0xFF10B981), // Emerald
    juce::Colour(0xFFEF4444), // Red
    juce::Colour(0xFF3B82F6), // Blue
    juce::Colour(0xFFF97316), // Orange
    juce::Colour(0xFF8B5CF6), // Violet
    juce::Colour(0xFF06B6D4), // Teal
    juce::Colour(0xFFEC4899), // Pink
    juce::Colour(0xFF84CC16), // Lime
    juce::Colour(0xFF6366F1), // Indigo
};

SpatialMapComponent::SpatialMapComponent() = default;

void SpatialMapComponent::paint(juce::Graphics& /*g*/)
{
    // Stub — real spatial map rendering during extraction
}

void SpatialMapComponent::mouseDown(const juce::MouseEvent& /*e*/)
{
    // Stub
}

void SpatialMapComponent::mouseDrag(const juce::MouseEvent& /*e*/)
{
    // Stub
}

void SpatialMapComponent::setObjectState(int index, float azimuthDeg, float elevationDeg,
                                          float distance, bool enabled)
{
    if (index >= 0 && index < MAX_SOURCES)
        objects[static_cast<size_t>(index)] = { azimuthDeg, elevationDeg, distance, enabled };
}

void SpatialMapComponent::setOscOverride(int index, bool active)
{
    if (index >= 0 && index < MAX_SOURCES)
        oscOverride[static_cast<size_t>(index)] = active;
}

void SpatialMapComponent::setSelectedObject(int index)
{
    selectedObject = index;
}

void SpatialMapComponent::addListener(Listener* l)    { listenerList.add(l); }
void SpatialMapComponent::removeListener(Listener* l) { listenerList.remove(l); }

void SpatialMapComponent::setObjectActivityLevel(int index, float level)
{
    if (index >= 0 && index < MAX_SOURCES)
        activityLevel[static_cast<size_t>(index)] = level;
}

} // namespace spatialcore
