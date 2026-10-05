#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <array>
#include <vector>
#include <utility>

namespace spatialcore
{

//==============================================================================
// 2D top-down spatial map showing object positions.
// Generic N-object component (Phase 9 Plan 09-03 / CUI-02): the object count is
// supplied via the constructor, not hardcoded to any DSP-side constant. Storage
// is capped at spatialcore::MAX_SOURCES (the SpatialCore-wide per-source
// ceiling used for backing-array sizing across the library), but only the
// first numObjects entries are ever iterated/rendered — a future consumer may
// pass a smaller N.
//==============================================================================
class SpatialMapComponent : public juce::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void objectPositionChanged (int objectIndex, float azimuthDeg, float distance) = 0;
        virtual void objectSelected (int objectIndex) = 0;
    };

    explicit SpatialMapComponent (int numObjects);

    /** Change the active object count at runtime. Must be <= MAX_SOURCES. */
    void setNumObjects (int numObjects) { numObjects_ = juce::jlimit (0, MAX_SOURCES, numObjects); }
    int getNumObjects() const { return numObjects_; }

    /** Provides random-trajectory look-ahead evaluation for the Random shape's
        trail rendering. Optional — if unset, Random-trajectory trails are
        simply not drawn (no crash). Owned by the caller; not retained beyond
        the pointer (matches OpenSpatialDelayProcessor* lifetime pattern this
        replaces — set once at construction, lives for the app's lifetime). */
    void setTrajectoryEngine (const TrajectoryEngine* engine) { trajectoryEngine = engine; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void setObjectState (int index, float azimuthDeg, float elevationDeg,
                         float distance, bool enabled);
    void setOscOverride (int index, bool active) { if (index >= 0 && index < numObjects_) oscOverride[(size_t)index] = active; }
    void setSelectedObject (int index) { selectedObject = index; repaint(); }

    /** Round-2 screenshot hook (Spatial-Media-Lab/OpenSpatialDelay#168): when true, the elevation-readout label
        is drawn next to every enabled tap rather than only the selected one.
        Used by the screenshot tool's elevation-map mode so the spiral of
        taps all display their in-plugin elevation label in the tap's colour. */
    void setLabelAllEnabledObjectsForScreenshot (bool enabled) { labelAllEnabledForScreenshot = enabled; }

    /** Round-3 screenshot hook (Spatial-Media-Lab/OpenSpatialDelay#168): when true, the selected tap's
        trajectory trail is drawn at full brightness along the entire
        sampled path instead of fading based on proximity to the moving
        dot. Used by the hero screenshot so the Infinity path is visible
        as a complete figure-∞ on the map. */
    void setDrawFullTrajectoryForScreenshot (bool enabled) { drawFullTrajectoryForScreenshot = enabled; }

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // Callbacks: fired on object drag start/end — used for gesture wrapping (issue E15)
    std::function<void (int objectIndex)> onDragStarted;
    std::function<void (int objectIndex)> onDragEnded;

    static constexpr int MAX_SOURCES = spatialcore::MAX_SOURCES;
    static const juce::Colour objectColours[MAX_SOURCES];

    /** Return the object's centre position within this component, in local
        pixel coordinates. Used by the screenshot tool to overlay labels. */
    juce::Point<float> getObjectScreenPos (int index) const
    {
        if (index < 0 || index >= numObjects_) return {};
        const auto& o = objects[(size_t)index];
        return spatialToPixel (o.azimuthDeg, o.distance);
    }
    float getObjectElevation (int index) const
    {
        if (index < 0 || index >= numObjects_) return 0.0f;
        return objects[(size_t)index].elevationDeg;
    }
    bool isObjectEnabled (int index) const
    {
        if (index < 0 || index >= numObjects_) return false;
        return objects[(size_t)index].enabled;
    }
    void setTrajectoryState (int index, const TrajectoryEngine::TrajectoryState& ts) { if (index >= 0 && index < numObjects_) trajectoryStates[(size_t)index] = ts; }
    void advanceStarAnimation (float dt) { starTime += dt; }
    void setObjectActivityLevel (int index, float level) { if (index >= 0 && index < numObjects_) activityLevel[(size_t)index] = level; }
    void advancePulsePhases (float dt)
    {
        for (int i = 0; i < numObjects_; ++i)
        {
            if (activityLevel[(size_t)i] > 0.01f)
            {
                pulsePhase[(size_t)i] += dt / 3.0f;  // 3-second cycle
                if (pulsePhase[(size_t)i] >= 1.0f) pulsePhase[(size_t)i] -= 1.0f;
            }
            else
            {
                pulsePhase[(size_t)i] = 0.0f;
            }
        }
    }

private:
    struct ObjectInfo
    {
        float azimuthDeg  = 0.0f;
        float elevationDeg = 0.0f;
        float distance    = 0.5f;
        bool  enabled     = false;
    };

    int numObjects_ = 0;

    // The map owns its embedded JetBrains Mono so it renders the SML fonts under any look-and-feel
    // (D-06). JUCE resolves a typeface-less Font through the default look-and-feel, which a
    // component-level setLookAndFeel does not change, so without these the labels would fall back
    // to the system sans. The bytes are the same ones SMLLookAndFeel loads, so the render under
    // SML is unchanged.
    juce::Typeface::Ptr monoRegular_, monoMedium_, monoBold_;

    std::array<ObjectInfo, MAX_SOURCES> objects = {};
    std::array<TrajectoryEngine::TrajectoryState, MAX_SOURCES> trajectoryStates = {};  // v0.9: trajectory origin + state
    std::array<bool, MAX_SOURCES> oscOverride = {};  // v0.6: per-object OSC override indicator
    int selectedObject = -1;
    int draggedObject  = -1;
    bool labelAllEnabledForScreenshot = false;  // issue Spatial-Media-Lab/OpenSpatialDelay#168 round 2: elevation-map mode
    bool drawFullTrajectoryForScreenshot = false;  // issue Spatial-Media-Lab/OpenSpatialDelay#168 round 3: hero screenshot
    const TrajectoryEngine* trajectoryEngine = nullptr;  // for Random trail look-ahead

    juce::ListenerList<Listener> listeners;

    // Coordinate conversion
    juce::Point<float> spatialToPixel (float azimuthDeg, float distance) const;
    std::pair<float, float> pixelToSpatial (juce::Point<float> pixel) const;
    int findObjectAt (juce::Point<float> pos) const;

    // v0.7: Star field — deterministic twinkling dots
    struct Star { float x, y, phase, speed, size; juce::Colour colour; };
    std::vector<Star> stars;
    float starTime = 0.0f;
    void generateStars();

    // v0.7: Per-object activity level (0..1) for glow animation
    std::array<float, MAX_SOURCES> activityLevel = {};
    // v0.7: Per-object pulse phase (0..1, cycles at ~3s) for expanding ring effect
    std::array<float, MAX_SOURCES> pulsePhase = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpatialMapComponent)
};

} // namespace spatialcore
