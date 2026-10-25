#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace spatialcore
{

//==============================================================================
// PresetSaveOverlay — the "save preset" name prompt.
// Phase 9 Plan 09-04 (CUI-04): re-homed verbatim from OSD (self-contained,
// portable as-is); contract documented 2026-10-25, SpatialCore#13.
//
// show (existingName, parentEditor) opens the card as its own native window
// (addToDesktop, so the host does not swallow the typing; Issue
// Spatial-Media-Lab/OpenSpatialDelay#35), centred on parentEditor, modal
// without blocking, with existingName selected. It borrows parentEditor's
// look-and-feel.
//
// onSave (name) is called once, with the trimmed text, when the user presses
// Save or Return with a non-empty name; the overlay then closes. Cancel,
// Escape or a click outside close it without calling onSave. The overlay
// checks nothing beyond trimming and refusing an empty name (no file-name
// rules, no duplicate check) and writes nothing. What the name becomes (a
// file, a folder, overwrite rules) is the consumer's, and the consumer should
// clean it before using it in a file path (strip path separators and
// characters the file system rejects).
//
// showForSnapshot() attaches the card as a child of parentEditor instead, for
// headless screenshots. Message thread only.
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

    // Called with the trimmed, non-empty name (see the class comment).
    // SpatialCore saves nothing; the consumer decides where and how
    // (OpenSpatialDelay writes it to its User/ folder).
    std::function<void (const juce::String&)> onSave;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void inputAttemptWhenModal() override;  // click outside native window → dismiss

private:
    juce::TextEditor nameEditor;
    juce::TextButton saveBtn { "Save" }, cancelBtn { "Cancel" };

    static constexpr int cardW = 260, cardH = 130;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetSaveOverlay)
};

//==============================================================================
// PresetBrowser — preset-menu chrome, nothing more (Phase 9 Plan 09-04,
// CUI-04). Extracted from OpenSpatialDelay's showPresetMenu(); contract
// documented 2026-10-25, SpatialCore#13; consumer guide:
// docs/integration-guide.md "PresetBrowser contract".
//
// What it does: shows a category/preset popup built from a consumer-supplied
// list of Entry rows, reports the user's pick by echoing that row's
// originalIndex, and owns a PresetSaveOverlay that hands back a typed name.
//
// What it does not do: it stores no presets and keeps no state. There is no
// state format, no file I/O, no save/load hook and no versioning in
// SpatialCore. Where presets live, how they are written and read, and how an
// old preset is upgraded are the consumer's (OpenSpatialDelay keeps its
// PresetData in its own repo per its charter D-03; the OpenSpatialPanner
// state-format share is ROADMAP Phase 11, SpatialCore#2, built on this
// contract).
//
// The consumer supplies (the menu is built in list order):
//  - entries grouped by category: a new submenu starts whenever the category
//    differs from the previous entry's, so a category split across the list
//    appears twice;
//  - factory entries before user entries within a category: one separator is
//    drawn before the first user entry that follows a factory entry;
//  - a non-empty category: an entry with an empty category is not shown;
//  - an originalIndex that is unique and >= 0: the popup item ID is
//    originalIndex + 1, JUCE reserves ID 0 for "nothing picked" (an index of
//    -1 asserts in Debug) and a negative index cannot be picked. Two entries
//    with the same index report the same pick.
//
// It gets back: onSelect (originalIndex) once, after the user picks an entry,
// and nothing when the menu is dismissed. A "User" submenu is always shown;
// when no entry has the category "User" (exact, case-sensitive) it holds one
// disabled "(empty)" item.
//
// Threading: message thread only, like any juce::Component; never call it
// from the audio thread. showMenu() returns at once (PopupMenu::showMenuAsync)
// and onSelect runs later, on the message thread.
//
// Lifetime: the open menu works from its own copy of the entries and keeps
// only a copy of onSelect, so destroying the PresetBrowser does not affect it.
// JUCE holds the target component and the look-and-feel through weak
// references, but the menu is not told to watch the consumer's editor for
// deletion, so an onSelect that touches the editor should capture a
// juce::Component::SafePointer to it (or the editor's destructor should call
// juce::PopupMenu::dismissAllActiveMenus()). Keep the PresetBrowser as a
// member of the editor.
//==============================================================================
class PresetBrowser
{
public:
    PresetBrowser() = default;

    /** One row of preset-browser content, content-agnostic (no delay-specific
        fields). The consumer converts its own preset list to this shape; see
        the class comment for the ordering rules. */
    struct Entry
    {
        juce::String category;   // submenu name; must not be empty
        juce::String name;       // item text
        int originalIndex = 0;   // consumer-defined, unique, >= 0; echoed back by onSelect
        bool isFactory = false;  // factory entries come before user entries within a category
    };

    /** Build and show the nested category/preset PopupMenu, mirroring the
        real plugin's popup: one submenu per run of equal categories, a
        separator before the first user entry that follows a factory entry,
        the current entry ticked, and an always-present (possibly empty)
        "User" category (Issue Spatial-Media-Lab/OpenSpatialDelay#2).
        Returns at once; the menu is shown asynchronously. Message thread only.
        @param entries          content supplied by the consumer, in display
                                 order (see the class comment).
        @param currentIndex     originalIndex of the active preset, which is
                                 ticked; a value no entry uses (for example
                                 -1) ticks nothing.
        @param targetComponent  anchor; the menu opens below it. JUCE holds it
                                 through a weak reference.
        @param lookAndFeel      look-and-feel for the popup. JUCE holds it
                                 through a weak reference.
        @param onSelect         called once, on the message thread, with the
                                 picked entry's originalIndex; not called when
                                 the menu is dismissed. Copied into the open
                                 menu. */
    void showMenu (const std::vector<Entry>& entries,
                   int currentIndex,
                   juce::Component& targetComponent,
                   juce::LookAndFeel& lookAndFeel,
                   std::function<void (int)> onSelect);

    /** Access the embedded PresetSaveOverlay: set its onSave and call
        show (currentName, editor) to ask for a name, or showForSnapshot for
        screenshots. The overlay lives as long as this PresetBrowser. */
    PresetSaveOverlay& getSaveOverlay() { return saveOverlay; }

private:
    PresetSaveOverlay saveOverlay;
    juce::PopupMenu activeMenu;  // the last menu shown; JUCE's popup window works from its own copy

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace spatialcore
