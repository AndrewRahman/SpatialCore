#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace spatialcore
{

class SpatialMapComponent : public juce::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void objectPositionChanged(int objectIndex, float azimuthDeg, float distance) = 0;
        virtual void objectSelected(int objectIndex) = 0;
    };

    SpatialMapComponent();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void setObjectState(int index, float azimuthDeg, float elevationDeg,
                        float distance, bool enabled);
    void setOscOverride(int index, bool active);
    void setSelectedObject(int index);
    void addListener(Listener* l);
    void removeListener(Listener* l);

    void setObjectActivityLevel(int index, float level);

    static const juce::Colour objectColours[MAX_SOURCES];

private:
    struct ObjectInfo
    {
        float azimuthDeg   = 0.0f;
        float elevationDeg = 0.0f;
        float distance     = 1.0f;
        bool  enabled      = false;
    };

    std::array<ObjectInfo, MAX_SOURCES> objects = {};
    std::array<bool, MAX_SOURCES> oscOverride   = {};
    std::array<float, MAX_SOURCES> activityLevel = {};
    int selectedObject = -1;
    int draggedObject  = -1;
    juce::ListenerList<Listener> listenerList;
};

} // namespace spatialcore
