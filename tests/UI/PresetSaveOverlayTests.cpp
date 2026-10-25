// PresetSaveOverlay: Escape in the name field (Phase 7 review WR-01).
//
// JUCE 9's TextEditor consumes Escape while it has focus (it posts an escape message and returns
// true from keyPressed), so the overlay's own keyPressed never sees it from the name field. The
// overlay wires nameEditor.onEscapeKey to dismiss(). The editor delivers that callback through a
// posted message, and UI tests never pump the message loop, so this test calls the callback
// directly: it proves the wiring, not JUCE's posting.

#include <catch2/catch_test_macros.hpp>

#include <SpatialCore/UI/PresetBrowser.h>

#include "UITestSupport.h"

using spatialcore::PresetSaveOverlay;

namespace
{
juce::TextEditor* findNameEditor (PresetSaveOverlay& overlay)
{
    for (auto* child : overlay.getChildren())
        if (auto* editor = dynamic_cast<juce::TextEditor*> (child))
            return editor;
    return nullptr;
}
}

TEST_CASE ("PresetSaveOverlay: Escape in the name field closes the overlay without saving",
           "[ui][preset]")
{
    juce::Component parent;
    parent.setSize (400, 300);

    PresetSaveOverlay overlay;
    int saveCalls = 0;
    overlay.onSave = [&saveCalls] (const juce::String&) { ++saveCalls; };
    overlay.showForSnapshot ("My preset", &parent);

    auto* editor = findNameEditor (overlay);
    REQUIRE (editor != nullptr);
    REQUIRE (editor->getText() == "My preset");
    REQUIRE (static_cast<bool> (editor->onEscapeKey));

    // The editor takes Escape for itself, so the overlay's own handler is never reached from here.
    CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));

    // dismiss() clears the name; this is the callback the editor fires for that Escape.
    editor->onEscapeKey();

    CHECK (editor->getText().isEmpty());
    CHECK (saveCalls == 0);
}
