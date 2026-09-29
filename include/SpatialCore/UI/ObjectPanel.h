#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <SpatialCore/UI/ReverseSlider.h>
#include <SpatialCore/UI/IndicatorToggle.h>
#include <SpatialCore/UI/StyledButton.h>
#include <SpatialCore/UI/SpatialMapComponent.h>

namespace spatialcore
{

//==============================================================================
// ObjectPanel — generic object tab row + spatial cluster building blocks
// (Phase 9 Plan 09-06, CUI-03). No prior analog existed: this is a NEW
// composite container design, not an extraction of an existing class (see
// 09-RESEARCH.md Pitfall 1). The scattered widgets it replaces were direct
// members of OSD's OpenSpatialDelayEditor (PluginEditor.h:272-297) with
// grey-out/color logic in updateObjectButtonColours() and per-object APVTS
// attachments rebound in selectObject() (both PluginEditor.cpp).
//
// TASK 1 GO/NO-GO OUTCOME: the slot-based injection API (a mechanism for a
// future consumer to append extra columns to the spatial cluster) was
// attempted and IS present here (see addExtraColumn()/getExtraColumnArea()),
// but ObjectPanel follows the WIDGET-OWNERSHIP pattern established by Plan
// 09-05's IOSectionComponent/OSCSectionComponent, not GlobalTapDrawer's pure
// setContentComponent() swap: the tab row + spatial cluster widgets
// (AZIM/ELEV/DIST/TRAJ/SPEED/DIR/ON, plus INPUT-CHANNEL) are themselves
// generic stock JUCE controls with a single shared dynamic-width layout
// algorithm across all of them — there is no single "content component" to
// swap the way GlobalTapDrawer's 6-knob panel was. Per-object APVTS
// attachments are NOT owned here (binding stays OSD-side, D-03); OSD rebinds
// them against the exposed widget accessors inside its own selectObject().
//==============================================================================
class ObjectPanel : public juce::Component
{
public:
    /** numObjects mirrors the Plan-03 SpatialMapComponent genericity decision:
        an explicit runtime/constructor object count, decoupled from any DSP
        engine constant (RESOLVED Open Question 1, 09-RESEARCH.md). Must be
        <= MAX_SOURCES. */
    ObjectPanel (SMLLookAndFeel& lookAndFeel, int numObjects);

    void resized() override;

    int getNumObjects() const { return numObjects_; }

    /** Which tab is currently highlighted as selected. Does not itself fire
        any parameter change — purely a visual/layout concern. The consumer
        (OSD) still owns "what object is selected" as plugin state and calls
        this to keep the panel's chrome in sync after its own selectObject(). */
    void setSelectedObject (int index);
    int getSelectedObject() const { return selectedObject; }

    /** Generic per-object color + grey-out (disabled-object dimming), moved
        byte-faithfully from OSD's updateObjectButtonColours() (D-09: move,
        don't improve). isEnabled(i) is supplied by the consumer via a
        callback since "enabled" is an APVTS-backed plugin concept ObjectPanel
        must not read directly (binding stays OSD-side). */
    std::function<bool (int)> isObjectEnabled;
    void updateButtonColours();

    // --- Tab row -----------------------------------------------------------
    juce::TextButton& getObjectButton (int index) { return objectButtons[(size_t) index]; }

    // --- Spatial cluster widget accessors (OSD attaches APVTS bindings /
    //     onChange / onClick / manual gesture-bracket callbacks to these
    //     after construction — mirrors IOSectionComponent/OSCSectionComponent) --
    ReverseSlider&  getAzimuthSlider()   { return azimuthSlider; }
    juce::Slider&   getElevationSlider() { return elevationSlider; }
    juce::Slider&   getDistanceSlider()  { return distanceSlider; }
    IndicatorToggle& getEnabledToggle()  { return *enabledToggle; }
    juce::Slider&   getDopplerSlider()   { return dopplerSlider; }   // DIR (Doppler amount)
    juce::ComboBox& getTrajectoryBox()   { return trajectoryBox; }
    juce::Slider&   getTrajectorySpeedSlider() { return trajectorySpeedSlider; }

    juce::Label& getAzimuthLabel()   { return azimuthLabel; }
    juce::Label& getElevationLabel() { return elevationLabel; }
    juce::Label& getDistanceLabel()  { return distanceLabel; }
    juce::Label& getDopplerLabel()   { return dopplerLabel; }
    juce::Label& getTrajectoryLabel() { return trajectoryLabel; }
    juce::Label& getTrajectorySpeedLabel() { return trajectorySpeedLabel; }

    /** Slot injection point (attempted per RESEARCH Open Question 4): a
        future consumer (e.g. OSP) may add an extra owned column to the right
        of the fixed AZIM/ELEV/DIST/.../SPEED cluster. OSD injects zero extra
        columns (it uses only the base set). The base cluster's own layout
        math is entirely unaffected by whether this is ever called. */
    void addExtraColumn (juce::Component* ownedColumn, int preferredWidth);

    static constexpr int kKnobDiameter = 46;
    static constexpr int kGroupGap = 10;

private:
    SMLLookAndFeel& lookAndFeel;
    int numObjects_ = 0;
    int selectedObject = 0;

    std::array<juce::TextButton, MAX_SOURCES> objectButtons;

    ReverseSlider   azimuthSlider;
    juce::Slider    elevationSlider;
    juce::Slider    distanceSlider;
    std::unique_ptr<IndicatorToggle> enabledToggle;
    juce::Slider    dopplerSlider;
    juce::ComboBox  trajectoryBox;
    juce::Slider    trajectorySpeedSlider;

    juce::Label azimuthLabel, elevationLabel, distanceLabel, dopplerLabel;
    juce::Label trajectoryLabel, trajectorySpeedLabel;

    // Slot-injected extra columns (owned by ObjectPanel once injected, laid
    // out after the fixed cluster in resized()).
    struct ExtraColumn
    {
        std::unique_ptr<juce::Component> component;
        int preferredWidth = 0;
    };
    std::vector<ExtraColumn> extraColumns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ObjectPanel)
};

} // namespace spatialcore
