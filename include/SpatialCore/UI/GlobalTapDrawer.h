#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <SpatialCore/UI/ReverseSlider.h>

namespace spatialcore
{

//==============================================================================
// GlobalTapDrawer — collapsible left-edge mini-drawer container.
// Phase 9 Plan 09-04 (CUI-04): re-extracted from OSD's GlobalTapDrawerComponent.
// Container chrome (handle, slide animation, paint) is fully generic; the
// knob content (OSD's 6 delay-plugin knobs: az/el/dist/pitch/doppler/speed)
// is injected by the consumer via setContentComponent() rather than being
// owned by this class — the KnobPanel injection boundary already existed in
// the OSD source (PluginEditor.h:519-522) and is formalized here.
//==============================================================================
class GlobalTapDrawer : public juce::Component, private juce::Timer
{
public:
    explicit GlobalTapDrawer (SMLLookAndFeel& lookAndFeel);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    bool isOpen() const { return open; }
    void setOpen (bool shouldBeOpen, bool animate = false);

    /** Current animated width — use for layout in the parent editor. */
    int getCurrentWidth() const { return currentWidth; }

    /** Inject the consumer's content component (e.g. OSD's 6-knob panel).
        The drawer clips this component by setting its bounds to only the
        visible portion, creating the slide-in/out effect. Not owned. */
    void setContentComponent (juce::Component* content);

    // Callback: fired when drawer toggles open/close (called on each animation frame)
    std::function<void()> onToggle;

    static constexpr int kClosedWidth = 14;
    static constexpr int kOpenWidth   = 78;
    static constexpr int kHandleWidth = 16;
    static constexpr int kPanelWidth  = kOpenWidth - kHandleWidth;  // 62px content area

private:
    bool open = false;
    bool handleHover = false;
    SMLLookAndFeel& lookAndFeel;

    juce::Component* contentComponent = nullptr;  // not owned; injected by consumer

    // Animation state
    int currentWidth = kClosedWidth;
    int targetWidth  = kClosedWidth;
    void timerCallback() override;

    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalTapDrawer)
};

} // namespace spatialcore
