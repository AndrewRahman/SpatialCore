#include <SpatialCore/UI/PresetBrowser.h>
#include "SpatialCoreUIFontData.h"

namespace spatialcore
{

namespace
{
    // Colours matching OSD's Colours_OSD namespace (Source/PluginEditor.cpp)
    const juce::Colour bgPanel      (0xff0a0d12);
    const juce::Colour bgRecessed   (0xff010205);
    const juce::Colour borderSubtle (0xff252930);
    const juce::Colour borderDim    (0xff171b20);
    const juce::Colour accentStellar(0xff80d8ff);
    const juce::Colour textDim      (0xff6d7279);
}

//==============================================================================
// PresetSaveOverlay — native popup window for saving presets.
//==============================================================================
PresetSaveOverlay::PresetSaveOverlay()
{
    titleTypeface_ = juce::Typeface::createSystemTypefaceFor (SpatialCoreUIFontData::DM_SansBold_ttf, SpatialCoreUIFontData::DM_SansBold_ttfSize);

    setWantsKeyboardFocus (true);
    // v1.0: Do NOT mark opaque — rounded rectangle leaves corner pixels unpainted.
    // Opaque flag with unpainted regions causes corrupted CoreAnimation backing store
    // → Metal GPU crash (EXC_BAD_ACCESS in AGXMetalG16X). See issue Spatial-Media-Lab/OpenSpatialDelay#37.

    // Name editor — DM Sans Regular 13px, recessed bg, cyan focus outline
    nameEditor.setMultiLine (false);
    nameEditor.setReturnKeyStartsNewLine (false);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, bgRecessed);
    nameEditor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour (juce::TextEditor::outlineColourId, borderDim);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, accentStellar);
    nameEditor.setColour (juce::TextEditor::highlightColourId, accentStellar.withAlpha (0.35f));
    nameEditor.setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    nameEditor.setJustification (juce::Justification::centred);
    nameEditor.setTextToShowWhenEmpty ("Enter preset name...", textDim);
    nameEditor.onReturnKey = [this]
    {
        auto name = nameEditor.getText().trim();
        if (name.isNotEmpty() && onSave)
        {
            onSave (name);
            dismiss();
        }
    };
    addAndMakeVisible (nameEditor);

    // Save button — filled cyan, dark text (primary action)
    saveBtn.setColour (juce::TextButton::buttonColourId, accentStellar);
    saveBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0a0d12));
    saveBtn.onClick = [this]
    {
        auto name = nameEditor.getText().trim();
        if (name.isNotEmpty() && onSave)
        {
            onSave (name);
            dismiss();
        }
    };
    addAndMakeVisible (saveBtn);

    // Cancel button — unfilled, dark red-tinted bg, red text (destructive/dismiss action)
    cancelBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff120508));
    cancelBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffed5e5e));
    cancelBtn.onClick = [this] { dismiss(); };
    addAndMakeVisible (cancelBtn);
}

void PresetSaveOverlay::show (const juce::String& existingName, juce::Component* parentEditor)
{
    nameEditor.setText (existingName, false);

    // Inherit LookAndFeel from parent editor for consistent styling
    if (parentEditor != nullptr)
        setLookAndFeel (&parentEditor->getLookAndFeel());

    // Set bounds BEFORE addToDesktop — ensures valid peer creation (Issue Spatial-Media-Lab/OpenSpatialDelay#35 fix)
    setSize (cardW, cardH);
    if (parentEditor != nullptr)
    {
        auto editorBounds = parentEditor->getScreenBounds();
        int x = editorBounds.getX() + (editorBounds.getWidth()  - cardW) / 2;
        int y = editorBounds.getY() + (editorBounds.getHeight() - cardH) / 2;
        setTopLeftPosition (x, y);
    }

    // Create native OS window — bypasses host keyboard interception
    addToDesktop (juce::ComponentPeer::windowIsTemporary
                | juce::ComponentPeer::windowHasDropShadow);

    setAlwaysOnTop (true);  // prevent z-order issues in some hosts (Issue Spatial-Media-Lab/OpenSpatialDelay#35 fix)
    setVisible (true);
    toFront (true);
    enterModalState (true);  // non-blocking modal; inputAttemptWhenModal() fires on outside clicks
    nameEditor.grabKeyboardFocus();
    nameEditor.setHighlightedRegion ({ 0, nameEditor.getText().length() });
}

void PresetSaveOverlay::showForSnapshot (const juce::String& existingName,
                                         juce::Component* parentEditor)
{
    nameEditor.setText (existingName, false);

    if (parentEditor != nullptr)
        setLookAndFeel (&parentEditor->getLookAndFeel());

    setSize (cardW, cardH);

    if (parentEditor != nullptr)
    {
        setTopLeftPosition ((parentEditor->getWidth()  - cardW) / 2,
                            (parentEditor->getHeight() - cardH) / 2);
        parentEditor->addAndMakeVisible (this);
    }

    setVisible (true);
}

void PresetSaveOverlay::dismiss()
{
    if (isCurrentlyModal())
        exitModalState (0);

    if (isOnDesktop())
        removeFromDesktop();

    nameEditor.clear();
}

void PresetSaveOverlay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fill entire bounds first for CoreAnimation safety (issue Spatial-Media-Lab/OpenSpatialDelay#37)
    g.fillAll (bgPanel);

    // Card background (rounded corners painted over the fill)
    g.setColour (bgPanel);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Card border
    g.setColour (borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    // Title — SAVE PRESET
    g.setColour (textDim);
    auto titleFont = juce::Font (juce::FontOptions (titleTypeface_).withHeight (13.0f));
    g.setFont (titleFont);
    auto titleArea = bounds.withHeight (28.0f).translated (0.0f, 10.0f);
    g.drawText ("SAVE PRESET", titleArea.toNearestInt(), juce::Justification::centred);
}

void PresetSaveOverlay::resized()
{
    int pad = 24;

    // Name editor
    nameEditor.setBounds (pad, 42, getWidth() - pad * 2, 26);

    // Buttons
    int btnW = 76, btnH = 26, btnGap = 10;
    int totalBtnW = btnW * 2 + btnGap;
    int btnX = (getWidth() - totalBtnW) / 2;
    int btnY = getHeight() - btnH - 14;
    saveBtn.setBounds (btnX, btnY, btnW, btnH);
    cancelBtn.setBounds (btnX + btnW + btnGap, btnY, btnW, btnH);
}

bool PresetSaveOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }
    return false;
}

void PresetSaveOverlay::inputAttemptWhenModal()
{
    // Click outside the native window → dismiss without saving
    dismiss();
}

//==============================================================================
// PresetBrowser — generic preset-menu popup chrome
// (formerly inline OpenSpatialDelayEditor::showPresetMenu()).
//==============================================================================
void PresetBrowser::showMenu (const std::vector<Entry>& entries,
                               int currentIndex,
                               juce::Component& targetComponent,
                               juce::LookAndFeel& lookAndFeel,
                               std::function<void (int)> onSelect)
{
    juce::PopupMenu mainMenu;

    juce::String lastCategory;
    juce::PopupMenu currentSubMenu;
    bool hasFactoryInSub = false;
    bool hasUserInSub = false;

    auto flushSubMenu = [&]()
    {
        if (lastCategory.isNotEmpty() && (hasFactoryInSub || hasUserInSub))
            mainMenu.addSubMenu (lastCategory, currentSubMenu);
        currentSubMenu = juce::PopupMenu();
        hasFactoryInSub = false;
        hasUserInSub = false;
    };

    for (const auto& p : entries)
    {
        if (p.category != lastCategory)
        {
            flushSubMenu();
            lastCategory = p.category;
        }

        // Separator between factory and user presets within a category
        if (! p.isFactory && ! hasUserInSub && hasFactoryInSub)
            currentSubMenu.addSeparator();

        bool isCurrent = (p.originalIndex == currentIndex);
        currentSubMenu.addItem (p.originalIndex + 1,  // PopupMenu IDs are 1-based
                                p.name,
                                true,       // enabled
                                isCurrent); // ticked if current

        if (p.isFactory) hasFactoryInSub = true;
        else             hasUserInSub = true;
    }
    flushSubMenu();  // flush last category

    // v0.9: Always show "User" category even if empty (Issue Spatial-Media-Lab/OpenSpatialDelay#2)
    if (lastCategory != "User")
    {
        bool userFound = false;
        for (const auto& p : entries)
            if (p.category == "User") { userFound = true; break; }
        if (! userFound)
        {
            juce::PopupMenu emptyUserMenu;
            emptyUserMenu.addItem (-1, "(empty)", false);
            mainMenu.addSubMenu ("User", emptyUserMenu);
        }
    }

    mainMenu.setLookAndFeel (&lookAndFeel);
    activeMenu = mainMenu;
    activeMenu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetComponent (&targetComponent)
            .withMinimumWidth (160)
            .withPreferredPopupDirection (
                juce::PopupMenu::Options::PopupDirection::downwards),
        [onSelect] (int result)
        {
            if (result > 0 && onSelect)
                onSelect (result - 1);
        });
}

} // namespace spatialcore
