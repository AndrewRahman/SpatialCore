#include <SpatialCore/UI/GlobalTapDrawer.h>

namespace spatialcore
{

namespace
{
    // Colours matching OSD's Colours_OSD namespace (Source/PluginEditor.cpp)
    const juce::Colour borderDim       (0xff171b20);
    const juce::Colour accentGlobal    (0xffc8d8e8);
    const juce::Colour accentGlobalDim (0xff7d8894);
}

//==============================================================================
// GlobalTapDrawer — collapsible left-edge mini-drawer (container chrome only).
// Architecture: this component = handle + clipping viewport for the injected
// content component. The content is always full-sized; this component clips
// it by setting the content's visible bounds, creating a natural slide-in/out
// effect.
//==============================================================================
GlobalTapDrawer::GlobalTapDrawer (SMLLookAndFeel& lf)
    : lookAndFeel (lf)
{
    setLookAndFeel (&lf);
}

void GlobalTapDrawer::setContentComponent (juce::Component* content)
{
    contentComponent = content;
    if (contentComponent != nullptr)
        addAndMakeVisible (contentComponent);
    resized();
}

void GlobalTapDrawer::setOpen (bool shouldBeOpen, bool animate)
{
    if (open == shouldBeOpen) return;
    open = shouldBeOpen;
    targetWidth = open ? kOpenWidth : kClosedWidth;

    if (animate)
    {
        startTimerHz (60);
    }
    else
    {
        currentWidth = targetWidth;
        if (onToggle) onToggle();
    }
    repaint();
}

void GlobalTapDrawer::timerCallback()
{
    int diff = targetWidth - currentWidth;
    if (std::abs (diff) <= 1)
    {
        currentWidth = targetWidth;
        stopTimer();
    }
    else
    {
        // Ease-out: 25% of remaining distance per frame (60fps ≈ 200ms settle)
        currentWidth += static_cast<int> (std::ceil (diff * 0.25f));
    }
    if (onToggle) onToggle();  // triggers parent resized() to update bounds
}

void GlobalTapDrawer::paint (juce::Graphics& g)
{
    // Draw the handle strip (rightmost portion of drawer, or the whole thing when closed)
    int handleX = getWidth() - kHandleWidth;
    if (handleX < 0) handleX = 0;
    int handleW = getWidth() - handleX;
    auto handleBounds = juce::Rectangle<int> (handleX, 0, handleW, getHeight());

    g.setColour (juce::Colour (0xff10141c).withAlpha (0.6f));
    g.fillRect (handleBounds);
    g.setColour (borderDim);
    g.drawVerticalLine (handleX, 0.0f, static_cast<float> (getHeight()));
    if (handleX + handleW < getWidth())
        g.drawVerticalLine (handleX + handleW - 1, 0.0f, static_cast<float> (getHeight()));

    // "GLOBAL" text + arrow triangles
    auto hoverCol = handleHover ? accentGlobal : accentGlobalDim;
    g.setColour (hoverCol);

    auto font = lookAndFeel.dmSansBold
                    ? juce::Font (juce::FontOptions (lookAndFeel.dmSansBold).withHeight (12.5f))
                    : juce::Font (juce::FontOptions (12.5f).withStyle ("Bold"));
    g.setFont (font);

    float cx = handleX + handleW * 0.5f;
    float cy = getHeight() * 0.5f;

    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, cx, cy));
    g.drawText ("GLOBAL", static_cast<int> (cx - 32), static_cast<int> (cy - 6), 64, 12,
                juce::Justification::centred, false);
    g.restoreState();

    // Arrow triangles above and below text (symmetric spacing)
    float arrowSize = 5.0f;
    float arrowGap  = 30.0f;
    float topArrowY = cy - arrowGap;
    float botArrowY = cy + arrowGap;
    bool pointRight = ! open;

    // Center arrow horizontally: shift tip so arrow midpoint aligns with handle center
    float arrowCx = cx + (pointRight ? arrowSize * 0.5f : -arrowSize * 0.5f);

    auto drawArrow = [&] (float tipX, float tipY, bool pr) {
        juce::Path arrow;
        float halfH = arrowSize * 0.55f;
        if (pr)
            arrow.addTriangle (tipX - arrowSize, tipY - halfH,
                               tipX - arrowSize, tipY + halfH, tipX, tipY);
        else
            arrow.addTriangle (tipX + arrowSize, tipY - halfH,
                               tipX + arrowSize, tipY + halfH, tipX, tipY);
        g.fillPath (arrow);
    };
    drawArrow (arrowCx, topArrowY, pointRight);
    drawArrow (arrowCx, botArrowY, pointRight);
}

void GlobalTapDrawer::resized()
{
    if (contentComponent == nullptr)
        return;

    // The content component is clipped to the visible panel area (left of handle).
    // Its internal size is always kPanelWidth × height, but we set its BOUNDS
    // to only show the visible portion, creating natural clipping.
    int visiblePanelW = getWidth() - kHandleWidth;
    if (visiblePanelW < 0) visiblePanelW = 0;

    // Position content so its RIGHT edge aligns with the handle's left edge.
    // When closing, the panel slides LEFT (negative x), hiding content progressively.
    int panelX = visiblePanelW - kPanelWidth;  // negative when partially closed
    contentComponent->setBounds (panelX, 0, kPanelWidth, getHeight());
}

void GlobalTapDrawer::mouseDown (const juce::MouseEvent& e)
{
    // Handle click area: everything to the right of the content panel
    int handleX = getWidth() - kHandleWidth;
    if (handleX < 0) handleX = 0;
    if (e.getPosition().x >= handleX)
        setOpen (! open, true);
}

void GlobalTapDrawer::mouseEnter (const juce::MouseEvent& e)
{
    int handleX = juce::jmax (0, getWidth() - kHandleWidth);
    if (e.getPosition().x >= handleX && ! handleHover)
    {
        handleHover = true;
        repaint();
    }
}

void GlobalTapDrawer::mouseExit (const juce::MouseEvent&)
{
    if (handleHover)
    {
        handleHover = false;
        repaint();
    }
}

void GlobalTapDrawer::mouseMove (const juce::MouseEvent& e)
{
    int handleX = juce::jmax (0, getWidth() - kHandleWidth);
    bool newHover = e.getPosition().x >= handleX;
    if (newHover != handleHover)
    {
        handleHover = newHover;
        repaint();
    }
}

} // namespace spatialcore
