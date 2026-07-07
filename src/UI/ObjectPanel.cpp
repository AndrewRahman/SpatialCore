#include <SpatialCore/UI/ObjectPanel.h>

namespace spatialcore
{

//==============================================================================
// ObjectPanel — generic object tab row + spatial cluster (chrome/layout +
// widgets only). Per-object APVTS attachments and "enabled" parameter state
// are NOT owned here (binding stays OSD-side, D-03) — the consumer attaches
// its own bindings to the exposed widget accessors after construction and
// supplies isObjectEnabled() so updateButtonColours() can compute grey-out
// without reading APVTS directly.
//==============================================================================
ObjectPanel::ObjectPanel (SMLLookAndFeel& lf, int numObjects)
    : lookAndFeel (lf),
      numObjects_ (juce::jlimit (0, MAX_SOURCES, numObjects))
{
    // --- Tab row ---
    for (int i = 0; i < numObjects_; ++i)
    {
        auto& btn = objectButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        btn.setClickingTogglesState (false);
        btn.setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (btn);
    }

    // --- Spatial cluster ---
    azimuthSlider.setLookAndFeel (&lookAndFeel);
    azimuthSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    azimuthSlider.setRotaryParameters (juce::MathConstants<float>::pi, 3.0f * juce::MathConstants<float>::pi, false);
    azimuthSlider.setReversed (true);  // IEM StereoEncoder convention: clockwise knob = clockwise on map
    azimuthSlider.setWantsKeyboardFocus (true);
    azimuthSlider.setMouseClickGrabsKeyboardFocus (true);
    azimuthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
    azimuthSlider.setScrollWheelEnabled (false);
    addAndMakeVisible (azimuthSlider);
    addAndMakeVisible (azimuthLabel);

    elevationSlider.setLookAndFeel (&lookAndFeel);
    elevationSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    elevationSlider.setRotaryParameters (juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
    elevationSlider.setWantsKeyboardFocus (true);
    elevationSlider.setMouseClickGrabsKeyboardFocus (true);
    elevationSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
    elevationSlider.setScrollWheelEnabled (false);
    addAndMakeVisible (elevationSlider);
    addAndMakeVisible (elevationLabel);

    distanceSlider.setLookAndFeel (&lookAndFeel);
    distanceSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    distanceSlider.setWantsKeyboardFocus (true);
    distanceSlider.setMouseClickGrabsKeyboardFocus (true);
    distanceSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
    distanceSlider.setScrollWheelEnabled (false);
    addAndMakeVisible (distanceSlider);
    addAndMakeVisible (distanceLabel);

    enabledToggle = std::make_unique<IndicatorToggle> ("ON", SpatialMapComponent::objectColours[0],
                                                       lookAndFeel.jetbrainsMedium);
    addAndMakeVisible (*enabledToggle);

    dopplerSlider.setLookAndFeel (&lookAndFeel);
    dopplerSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    dopplerSlider.setWantsKeyboardFocus (true);
    dopplerSlider.setMouseClickGrabsKeyboardFocus (true);
    dopplerSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
    dopplerSlider.setScrollWheelEnabled (false);
    addAndMakeVisible (dopplerSlider);
    addAndMakeVisible (dopplerLabel);

    trajectoryBox.setLookAndFeel (&lookAndFeel);
    trajectoryBox.addItem ("None",      1);
    trajectoryBox.addItem ("Bounce",    2);
    trajectoryBox.addItem ("Circle",    3);
    trajectoryBox.addItem ("Cross",     4);
    trajectoryBox.addItem ("Figure-8",  5);
    trajectoryBox.addItem ("Heart",     6);
    trajectoryBox.addItem ("Helix",     7);
    trajectoryBox.addItem ("Infinity",  8);
    trajectoryBox.addItem ("Line",      9);
    trajectoryBox.addItem ("Orbit",    10);
    trajectoryBox.addItem ("Random",   11);
    trajectoryBox.addItem ("Spiral",   12);
    trajectoryBox.addItem ("Square",   13);
    trajectoryBox.addItem ("Triangle", 14);
    addAndMakeVisible (trajectoryBox);
    addAndMakeVisible (trajectoryLabel);

    trajectorySpeedSlider.setLookAndFeel (&lookAndFeel);
    trajectorySpeedSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    trajectorySpeedSlider.setWantsKeyboardFocus (true);
    trajectorySpeedSlider.setMouseClickGrabsKeyboardFocus (true);
    trajectorySpeedSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
    trajectorySpeedSlider.setScrollWheelEnabled (false);
    addAndMakeVisible (trajectorySpeedSlider);
    addAndMakeVisible (trajectorySpeedLabel);
}

void ObjectPanel::setSelectedObject (int index)
{
    selectedObject = juce::jlimit (0, juce::jmax (0, numObjects_ - 1), index);
    for (int i = 0; i < numObjects_; ++i)
        objectButtons[(size_t) i].setToggleState (i == selectedObject, juce::dontSendNotification);
}

void ObjectPanel::updateButtonColours()
{
    // Moved byte-faithfully from OSD's updateObjectButtonColours() (D-09):
    // 4-state tap buttons (enabled/disabled x selected/unselected). Colour
    // constants (bg alpha, border alpha) are the exact values from the
    // original; only the "enabled" lookup source changed (callback instead
    // of a direct APVTS read, since binding stays OSD-side) and the
    // recessed/dim/text colours (formerly Colours_OSD::bgRecessed /
    // ::borderDim / ::textDim / ::textPrimary) now come from the current
    // LookAndFeel_V4 colour scheme so this container has no OSD-specific
    // colour dependency.
    auto& scheme = lookAndFeel.getCurrentColourScheme();
    auto bgRecessed  = scheme.getUIColour (juce::LookAndFeel_V4::ColourScheme::widgetBackground);
    auto borderDim   = scheme.getUIColour (juce::LookAndFeel_V4::ColourScheme::outline);
    auto textDim     = scheme.getUIColour (juce::LookAndFeel_V4::ColourScheme::defaultText).withAlpha (0.5f);
    auto textPrimary = scheme.getUIColour (juce::LookAndFeel_V4::ColourScheme::defaultText);

    for (int i = 0; i < numObjects_; ++i)
    {
        auto& btn = objectButtons[(size_t) i];
        auto colour = SpatialMapComponent::objectColours[i];

        bool enabled = isObjectEnabled ? isObjectEnabled (i) : false;
        bool selected = (i == selectedObject);

        juce::Colour bg, border, text;

        if (enabled && selected)
        {
            bg     = colour.withAlpha (0.4f);
            border = colour;
            text   = textPrimary;
        }
        else if (enabled && !selected)
        {
            bg     = colour.withAlpha (0.25f);
            border = colour.withAlpha (0.4f);
            text   = colour;
        }
        else if (!enabled && selected)
        {
            bg     = bgRecessed;
            border = colour;
            text   = colour;
        }
        else
        {
            bg     = bgRecessed;
            border = borderDim;
            text   = textDim;
        }

        btn.setColour (juce::TextButton::buttonColourId,  bg);
        btn.setColour (juce::TextButton::buttonOnColourId, bg);
        btn.setColour (juce::TextButton::textColourOffId, text);
        btn.setColour (juce::TextButton::textColourOnId,  text);
        juce::ignoreUnused (border);  // border colour reserved for a future outline-drawing LookAndFeel hook
    }
}

void ObjectPanel::addExtraColumn (juce::Component* ownedColumn, int preferredWidth)
{
    if (ownedColumn == nullptr)
        return;
    addAndMakeVisible (ownedColumn);
    extraColumns.push_back (ExtraColumn { std::unique_ptr<juce::Component> (ownedColumn), preferredWidth });
    resized();
}

void ObjectPanel::resized()
{
    // --- Tab row: fills the full width, N-1 gaps of 3px (mirrors the
    //     original inline math exactly: OSD PluginEditor.cpp resized()) ---
    auto area = getLocalBounds();
    auto tabRow = area.removeFromTop (22);
    if (numObjects_ > 0)
    {
        int btnGap = 3;
        int totalGaps = (numObjects_ - 1) * btnGap;
        int btnBase = (tabRow.getWidth() - totalGaps) / numObjects_;
        int btnRem  = (tabRow.getWidth() - totalGaps) % numObjects_;
        int btnX = tabRow.getX();
        for (int i = 0; i < numObjects_; ++i)
        {
            int w = btnBase + (i < btnRem ? 1 : 0);
            objectButtons[(size_t) i].setBounds (btnX, tabRow.getY(), w, tabRow.getHeight());
            btnX += w + btnGap;
        }
    }

    // --- Spatial cluster: dynamic knob width fills remaining space, mirrors
    //     the original "6 knobs + ON pill + TRAJECTORY column" cursor math.
    //     NOTE: the original inline layout also interleaved OSD-specific
    //     PITCH/INPUT-CHANNEL columns into this same shared-cursor
    //     computation (Source/PluginEditor.cpp resized(), bottom panel) —
    //     those stay OSD-side (not part of CUI-03's AZIM/ELEV/DIST/TRAJ/
    //     SPEED/DIR/ON scope) and are positioned by OSD alongside this
    //     panel's bounds, not inside it. ---
    auto clusterArea = area.removeFromTop (juce::jmax (0, area.getHeight()));
    int ctrlY = clusterArea.getY() + 8;
    int ctrlX = clusterArea.getX();
    int ctrlAvailW = clusterArea.getWidth();

    int onW = enabledToggle ? IndicatorToggle::getPreferredWidth (lookAndFeel.jetbrainsMedium, "ON") : 0;

    auto trajFont = juce::Font (juce::FontOptions (lookAndFeel.jetbrainsMedium).withHeight (10.0f).withKerningFactor (0.12f));
    juce::GlyphArrangement trajGa;
    trajGa.addLineOfText (trajFont, "TRAJECTORY", 0.0f, 0.0f);
    int trajLabelW = juce::roundToInt (std::ceil (trajGa.getBoundingBox (0, -1, false).getWidth())) + 6;

    auto comboFont = juce::Font (juce::FontOptions (lookAndFeel.dmSansRegular).withHeight (13.0f));
    juce::GlyphArrangement itemGa;
    itemGa.addLineOfText (comboFont, "Figure-8", 0.0f, 0.0f);
    int itemTextW = juce::roundToInt (std::ceil (itemGa.getBoundingBox (0, -1, false).getWidth())) + 24;
    int trajW = juce::jmax (trajLabelW, itemTextW);

    int extraW = 0;
    for (auto& col : extraColumns)
        extraW += col.preferredWidth + kGroupGap;

    // Base cluster has 4 knob columns (AZ/EL/DIST/SPEED) + ON pill + TRAJ box.
    int groupGap = kGroupGap;
    int fixedW = onW + 2 * groupGap + trajW + 4 + extraW;
    int knobBase = (ctrlAvailW - fixedW) / 4;
    int knobRem  = (ctrlAvailW - fixedW) % 4;
    auto knobColW = [knobBase, knobRem] (int col) -> int { return knobBase + (col < knobRem ? 1 : 0); };

    auto placeKnob = [this] (juce::Slider& slider, juce::Label& label, int x, int y, int colW)
    {
        label.setBounds (x, y, colW, 12);
        slider.setBounds (x + (colW - kKnobDiameter) / 2, y + 12, kKnobDiameter, 58);
    };

    if (enabledToggle)
        enabledToggle->setBounds (ctrlX, ctrlY, onW, IndicatorToggle::kHeight);
    ctrlX += onW + groupGap;

    { int w = knobColW (0); placeKnob (azimuthSlider,  azimuthLabel,  ctrlX, ctrlY, w); ctrlX += w; }
    { int w = knobColW (1); placeKnob (elevationSlider, elevationLabel, ctrlX, ctrlY, w); ctrlX += w; }
    { int w = knobColW (2); placeKnob (distanceSlider,  distanceLabel,  ctrlX, ctrlY, w); ctrlX += w; }

    ctrlX += groupGap;

    int trajStartX = ctrlX;
    {
        trajectoryLabel.setFont (trajFont);
        trajectoryLabel.setJustificationType (juce::Justification::centred);
        trajectoryLabel.setBorderSize (juce::BorderSize<int> (0));
        trajectoryLabel.setBounds (ctrlX, ctrlY, trajW, 12);
        trajectoryBox.setBounds (ctrlX, ctrlY + 14, trajW, 20);
        ctrlX += trajW + 4;
    }
    juce::ignoreUnused (trajStartX);

    { int w = juce::jmax (knobColW (3), kKnobDiameter + 4); placeKnob (dopplerSlider, dopplerLabel, ctrlX, ctrlY, w); ctrlX += w; }

    // Speed knob — right-aligned to the tab row's right edge (mirrors the
    // original "tapRightEdge" anchoring).
    {
        int speedW = juce::jmax (kKnobDiameter + 4, 46);
        int speedX = getLocalBounds().getRight() - speedW - extraW;
        placeKnob (trajectorySpeedSlider, trajectorySpeedLabel, speedX, ctrlY, speedW);
        ctrlX = speedX + speedW;
    }

    // Slot-injected extra columns, laid out after the fixed cluster.
    for (auto& col : extraColumns)
    {
        ctrlX += groupGap;
        col.component->setBounds (ctrlX, ctrlY, col.preferredWidth, 70);
        ctrlX += col.preferredWidth;
    }
}

} // namespace spatialcore
