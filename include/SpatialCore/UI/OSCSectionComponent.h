#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <SpatialCore/UI/IndicatorToggle.h>

namespace spatialcore
{

//==============================================================================
// OSCSectionComponent — generic OSC receive/send row container (Phase 9 Plan
// 09-05, CUI-04). No prior analog existed: this widget LAYOUT/chrome (toggle
// + editable-label positioning for the Receive and Send rows) was inline in
// OSD's OpenSpatialDelayEditor resized()/paint(). Per the Architectural
// Responsibility Map, this container owns only the generic widgets and their
// relative layout; OSC CONNECTION STATE / validation / listener binding stays
// plugin-side — the consumer attaches its own bindings to the exposed widget
// accessors after construction, mirroring the GlobalTapDrawer/GlobalTapKnobPanel
// injection split from Plan 09-04.
//
// The section header text ("OSC") itself is drawn by the shared
// drawSectionHeader() helper in OpenSpatialDelayEditor::paint() alongside the
// other right-panel sections (DELAY/MOD/TONE/MIX) and is NOT owned by this
// container. The "Port"/"IP" inline labels beside the toggles ARE part of
// this container's chrome and are drawn in its own paint().
//==============================================================================
class OSCSectionComponent : public juce::Component
{
public:
    OSCSectionComponent (const juce::String& receiveLabel, const juce::String& sendLabel,
                        const juce::Colour& accentColour, juce::Typeface::Ptr typeface);

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Colours used for the "Port"/"IP" inline text labels — set by the
        consumer to match its own theme (kept out of this container so it
        does not hardcode OSD's Colours_OSD palette). */
    void setLabelColour (const juce::Colour& colour) { labelColour = colour; }
    void setLabelFont (juce::Typeface::Ptr regularTypeface) { labelTypeface = regularTypeface; }

    static constexpr int kRowHeight   = 20;
    static constexpr int kRowGap      = 24;  // send row Y offset from receive row Y
    static constexpr int kPortLabelW  = 54;
    static constexpr int kIPLabelW    = 90;
    static constexpr int kSendPortW   = 48;
    static constexpr int kPortGap     = 34;  // gap between receive toggle and its port label
    static constexpr int kSendFieldGap = 4;  // gap between send toggle/IP/port fields

    // --- Widget accessors (OSD attaches OSC connection state / validation /
    //     onClick / onTextChange bindings to these after construction) ---
    IndicatorToggle& getReceiveToggle() { return receiveToggle; }
    juce::Label&     getReceivePortLabel() { return receivePortLabel; }
    IndicatorToggle& getSendToggle() { return sendToggle; }
    juce::Label&     getSendIPLabel() { return sendIPLabel; }
    juce::Label&     getSendPortLabel() { return sendPortLabel; }

private:
    juce::Colour labelColour = juce::Colours::grey;
    juce::Typeface::Ptr labelTypeface;

    juce::Typeface::Ptr toggleTypeface;  // retained for getPreferredWidth() in resized()

    IndicatorToggle receiveToggle;
    juce::Label     receivePortLabel;
    IndicatorToggle sendToggle;
    juce::Label     sendIPLabel;
    juce::Label     sendPortLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OSCSectionComponent)
};

} // namespace spatialcore
