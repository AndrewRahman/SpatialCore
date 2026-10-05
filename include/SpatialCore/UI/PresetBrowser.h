#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace spatialcore
{

//==============================================================================
// PresetSaveOverlay — native popup window for saving presets.
// Phase 9 Plan 09-04 (CUI-04): re-homed verbatim from OSD (self-contained,
// portable as-is). Uses addToDesktop() to bypass host keyboard interception
// (Issue Spatial-Media-Lab/OpenSpatialDelay#35).
//==============================================================================
class PresetSaveOverlay : public juce::Component
{
public:
    PresetSaveOverlay();

    void show (const juce::String& existingName, juce::Component* parentEditor);
    void dismiss();

    /** Attach the overlay as a child of parentEditor (not as a desktop window)
        and centre it. For headless screenshot capture. */
    void showForSnapshot (const juce::String& existingName,
                          juce::Component* parentEditor);

    // v1.0: callback with preset name only (always saves to User/)
    std::function<void (const juce::String&)> onSave;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void inputAttemptWhenModal() override;  // click outside native window → dismiss

private:
    juce::TextEditor nameEditor;
    juce::TextButton saveBtn { "Save" }, cancelBtn { "Cancel" };

    // Embedded DM Sans Bold for the SAVE PRESET title. A typeface-less Font would resolve through the
    // default look-and-feel, which a per-component setLookAndFeel does not change (D-22).
    juce::Typeface::Ptr titleTypeface_;

    static constexpr int cardW = 260, cardH = 130;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetSaveOverlay)
};

//==============================================================================
// PresetBrowser — generic preset-menu popup chrome (Phase 9 Plan 09-04, CUI-04).
// Extracted from OSD's inline OpenSpatialDelayEditor::showPresetMenu(). Owns
// only the PopupMenu-building/category-grouping machinery and the embedded
// PresetSaveOverlay; content (preset names/categories/current selection) is
// supplied by the consumer via the content-agnostic Entry list — delay-specific
// PresetData (Source/PresetData.h) stays in OSD per charter D-03.
//==============================================================================
class PresetBrowser
{
public:
    PresetBrowser() = default;

    /** One row of preset-browser content — content-agnostic (no delay-specific
        fields). Consumer converts its own preset list to this shape. */
    struct Entry
    {
        juce::String category;
        juce::String name;
        int originalIndex = 0;   // consumer-defined index, echoed back on selection
        bool isFactory = false;
    };

    /** Build and show the nested category/preset PopupMenu, mirroring the
        real plugin's popup: one submenu per category, factory presets first,
        a separator before user presets in the same category, current
        selection ticked, and an always-present (possibly empty) "User"
        category (Issue Spatial-Media-Lab/OpenSpatialDelay#2).
        @param entries          content supplied by the consumer.
        @param currentIndex     originalIndex of the currently active preset.
        @param targetComponent  anchor component for popup placement.
        @param lookAndFeel      look-and-feel to apply to the popup.
        @param onSelect         invoked with the selected entry's originalIndex
                                 when the user picks a preset (not invoked on
                                 dismiss-without-selection). */
    void showMenu (const std::vector<Entry>& entries,
                   int currentIndex,
                   juce::Component& targetComponent,
                   juce::LookAndFeel& lookAndFeel,
                   std::function<void (int)> onSelect);

    /** Access the embedded PresetSaveOverlay (for screenshot compositing and
        for the consumer to wire up onSave / show / showForSnapshot). */
    PresetSaveOverlay& getSaveOverlay() { return saveOverlay; }

private:
    PresetSaveOverlay saveOverlay;
    juce::PopupMenu activeMenu;  // kept alive for showMenuAsync's lifetime

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace spatialcore
