#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>

namespace spatialcore
{

//==============================================================================
// IOSectionComponent — generic I/O header-bar container (Phase 9 Plan 09-05,
// CUI-04). No prior analog existed: this widget LAYOUT/chrome (input/output
// format dropdowns + algorithm box positioning, label styling) was inline in
// OSD's OpenSpatialDelayEditor resized()/paint(). Per the Architectural
// Responsibility Map, this container owns only the generic widgets and their
// relative layout; PARAMETER BINDING (APVTS ComboBox attachments, dynamic
// item population, onChange handlers) stays plugin-side — the consumer
// attaches its own bindings to the exposed widget accessors after
// construction, mirroring the GlobalTapDrawer/GlobalTapKnobPanel injection
// split from Plan 09-04.
//
// Deliberately excluded from this container: the HRTF profile dropdown
// (hrtfProfileBox/hrtfProfileLabel) — it occupies the same on-screen slot as
// the algorithm box (mutually exclusive by visibility, toggled by OSD based
// on output format) but is a distinct OSD concern with no analog requested
// by CUI-04's scope ("format dropdowns + algorithm box").
//==============================================================================
class IOSectionComponent : public juce::Component
{
public:
    explicit IOSectionComponent (SMLLookAndFeel& lookAndFeel);

    void resized() override;

    /** Layout constants mirrored from the original inline header-row layout
        (OSD PluginEditor.cpp resized()). Widths are fixed; the consumer
        positions this component's overall bounds, this container arranges
        input/output/algorithm groups right-to-left within them. */
    static constexpr int kAlgoBoxWidth   = 110;
    static constexpr int kOutputBoxWidth = 110;
    static constexpr int kInputBoxWidth  = 70;
    static constexpr int kGap            = 6;
    static constexpr int kLabelHeight    = 12;
    static constexpr int kBoxHeight      = 22;

    // --- Widget accessors (OSD attaches APVTS bindings / onChange / dynamic
    //     item population to these after construction) ---
    juce::ComboBox& getInputFormatBox()   { return inputFormatBox; }
    juce::Label&    getInputFormatLabel() { return inputFormatLabel; }
    juce::ComboBox& getOutputFormatBox()   { return outputFormatBox; }
    const juce::ComboBox& getOutputFormatBox() const { return outputFormatBox; }
    juce::Label&    getOutputFormatLabel() { return outputFormatLabel; }
    juce::ComboBox& getAlgorithmBox()   { return algorithmBox; }
    juce::Label&    getAlgorithmLabel() { return algorithmLabel; }

private:
    SMLLookAndFeel& lookAndFeel;

    juce::ComboBox inputFormatBox;
    juce::Label    inputFormatLabel;
    juce::ComboBox outputFormatBox;
    juce::Label    outputFormatLabel;
    juce::ComboBox algorithmBox;
    juce::Label    algorithmLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IOSectionComponent)
};

} // namespace spatialcore
