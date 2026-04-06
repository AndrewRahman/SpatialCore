#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

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
    void setTrajectoryState(int index, const TrajectoryEngine::TrajectoryState& ts);
    void advanceStarAnimation(float dt) { starTime += dt; }
    void advancePulsePhases(float dt);

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
    std::array<TrajectoryEngine::TrajectoryState, MAX_SOURCES> trajectoryStates = {};
    std::array<bool, MAX_SOURCES> oscOverride   = {};
    std::array<float, MAX_SOURCES> activityLevel = {};
    std::array<float, MAX_SOURCES> pulsePhase = {};
    int selectedObject = -1;
    int draggedObject  = -1;
    juce::ListenerList<Listener> listenerList;

    // Coordinate conversion
    juce::Point<float> spatialToPixel(float azimuthDeg, float distance) const;
    std::pair<float, float> pixelToSpatial(juce::Point<float> pixel) const;
    int findObjectAt(juce::Point<float> pos) const;

    // Star field
    struct Star { float x, y, phase, speed, size; juce::Colour colour; };
    std::vector<Star> stars;
    float starTime = 0.0f;
    void generateStars();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpatialMapComponent)
};

} // namespace spatialcore
