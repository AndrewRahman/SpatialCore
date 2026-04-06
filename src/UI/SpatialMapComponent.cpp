#include <SpatialCore/UI/SpatialMapComponent.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>

namespace spatialcore
{

//==============================================================================
// Map colour constants (framework-independent)
//==============================================================================
namespace MapColours
{
    static const juce::Colour mapVoid     (0xff010204);
    static const juce::Colour textEtched  (0xff5a5e63);
    static const juce::Colour bgRecessed  (0xff010205);
    static const juce::Colour borderSubtle(0xff252930);
    static const juce::Colour borderDim   (0xff171b20);
    static const juce::Colour textDim     (0xff6d7279);
    static const juce::Colour accentStellar(0xff80d8ff);
}

//==============================================================================
// Object colours for the spatial map (12-hue rainbow)
//==============================================================================
const juce::Colour SpatialMapComponent::objectColours[MAX_SOURCES] = {
    juce::Colour(0xffED5E5E),  //  1 red
    juce::Colour(0xffEDA65E),  //  2 orange
    juce::Colour(0xffACD435),  //  3 lime
    juce::Colour(0xff3BCE6C),  //  4 green
    juce::Colour(0xff3CDDA7),  //  5 teal
    juce::Colour(0xff52E0E0),  //  6 cyan
    juce::Colour(0xff4DB3E6),  //  7 light blue
    juce::Colour(0xff6390E9),  //  8 blue
    juce::Colour(0xff6767E4),  //  9 indigo
    juce::Colour(0xffA667E4),  // 10 violet
    juce::Colour(0xffE467E4),  // 11 magenta
    juce::Colour(0xffE963A6),  // 12 rose
};

//==============================================================================
SpatialMapComponent::SpatialMapComponent() { generateStars(); }

void SpatialMapComponent::generateStars()
{
    stars.clear();
    uint32_t seed = 0x5ACEA10u;
    auto lcg = [&seed]() -> float {
        seed = seed * 1103515245u + 12345u;
        return (float)((seed >> 16) & 0x7FFF) / 32767.0f;
    };

    auto cyanWhite = juce::Colour(0xffe0f0fa);
    auto amber     = juce::Colour(0xfff0d8a8);
    auto violet    = juce::Colour(0xffc0a8f0);

    for (int i = 0; i < 80; ++i)
    {
        Star s;
        s.x = lcg();
        s.y = lcg();
        s.phase = lcg() * juce::MathConstants<float>::twoPi;
        s.speed = 0.7f + lcg() * 1.4f;
        s.size = lcg() < 0.80f ? 1.0f : 2.0f;

        float colRoll = lcg();
        if (colRoll < 0.82f)       s.colour = cyanWhite;
        else if (colRoll < 0.97f)  s.colour = amber;
        else                       s.colour = violet;

        stars.push_back(s);
    }
}

juce::Point<float> SpatialMapComponent::spatialToPixel(float azimuthDeg, float dist) const
{
    float radius = std::min(getWidth(), getHeight()) * 0.45f;
    float cx = getWidth()  * 0.5f;
    float cy = getHeight() * 0.5f;

    float azRad = juce::degreesToRadians(azimuthDeg);
    float x = cx - std::sin(azRad) * dist * radius;
    float y = cy - std::cos(azRad) * dist * radius;
    return { x, y };
}

std::pair<float, float> SpatialMapComponent::pixelToSpatial(juce::Point<float> pixel) const
{
    float radius = std::min(getWidth(), getHeight()) * 0.45f;
    float cx = getWidth()  * 0.5f;
    float cy = getHeight() * 0.5f;

    float dx = (pixel.x - cx) / radius;
    float dy = (pixel.y - cy) / radius;
    float dist = std::sqrt(dx * dx + dy * dy);
    dist = juce::jlimit(0.0f, 1.0f, dist);

    float azRad = std::atan2(-dx, -dy);
    float azDeg = juce::radiansToDegrees(azRad);

    return { azDeg, dist };
}

int SpatialMapComponent::findObjectAt(juce::Point<float> pos) const
{
    auto hitTest = [&](int i) -> bool
    {
        if (! objects[static_cast<size_t>(i)].enabled) return false;
        auto p = spatialToPixel(objects[static_cast<size_t>(i)].azimuthDeg,
                                objects[static_cast<size_t>(i)].distance);
        float elDeg = objects[static_cast<size_t>(i)].elevationDeg;
        float z = std::sin(juce::degreesToRadians(elDeg));
        float baseDiam = (i == selectedObject) ? 18.0f : 14.0f;
        float elevScale = (z >= 0.0f) ? 5.0f : 3.0f;
        float currentDotSize = baseDiam + elevScale * z;
        float hitRadius = std::max(currentDotSize * 0.5f + 2.0f, 10.0f);
        return p.getDistanceFrom(pos) < hitRadius;
    };

    if (selectedObject >= 0 && selectedObject < MAX_SOURCES && hitTest(selectedObject))
        return selectedObject;

    for (int i = MAX_SOURCES - 1; i >= 0; --i)
    {
        if (i == selectedObject) continue;
        if (hitTest(i))
            return i;
    }
    return -1;
}

void SpatialMapComponent::setObjectState(int index, float azimuthDeg, float elevationDeg,
                                          float distance, bool enabled)
{
    if (index >= 0 && index < MAX_SOURCES)
    {
        objects[static_cast<size_t>(index)] = { azimuthDeg, elevationDeg, distance, enabled };
        repaint();
    }
}

void SpatialMapComponent::setOscOverride(int index, bool active)
{
    if (index >= 0 && index < MAX_SOURCES)
        oscOverride[static_cast<size_t>(index)] = active;
}

void SpatialMapComponent::setSelectedObject(int index)
{
    selectedObject = index;
    repaint();
}

void SpatialMapComponent::addListener(Listener* l)    { listenerList.add(l); }
void SpatialMapComponent::removeListener(Listener* l) { listenerList.remove(l); }

void SpatialMapComponent::setObjectActivityLevel(int index, float level)
{
    if (index >= 0 && index < MAX_SOURCES)
        activityLevel[static_cast<size_t>(index)] = level;
}

void SpatialMapComponent::setTrajectoryState(int index, const TrajectoryEngine::TrajectoryState& ts)
{
    if (index >= 0 && index < MAX_SOURCES)
        trajectoryStates[static_cast<size_t>(index)] = ts;
}

void SpatialMapComponent::advancePulsePhases(float dt)
{
    for (int i = 0; i < MAX_SOURCES; ++i)
    {
        if (activityLevel[static_cast<size_t>(i)] > 0.01f)
        {
            pulsePhase[static_cast<size_t>(i)] += dt / 3.0f;
            if (pulsePhase[static_cast<size_t>(i)] >= 1.0f)
                pulsePhase[static_cast<size_t>(i)] -= 1.0f;
        }
        else
        {
            pulsePhase[static_cast<size_t>(i)] = 0.0f;
        }
    }
}

void SpatialMapComponent::paint(juce::Graphics& g)
{
    auto* lf = dynamic_cast<SMLLookAndFeel*>(&getLookAndFeel());
    float radius = std::min(getWidth(), getHeight()) * 0.45f;
    float cx = getWidth()  * 0.5f;
    float cy = getHeight() * 0.5f;

    // Background
    g.fillAll(MapColours::mapVoid);

    // Radial vignette
    {
        juce::ColourGradient vignette(MapColours::mapVoid.withAlpha(0.0f), cx, cy,
                                       juce::Colour(0xff000000).withAlpha(0.5f),
                                       cx + (float)getWidth() * 0.5f, cy, true);
        g.setGradientFill(vignette);
        g.fillRect(getLocalBounds());
    }

    // Star field
    {
        float w = (float)getWidth();
        float h = (float)getHeight();
        float globalPhase = std::fmod(starTime * 0.04f, 1.0f);

        for (auto& s : stars)
        {
            float twinkle = 0.15f + 0.50f * (0.5f + 0.5f * std::sin(starTime * s.speed + s.phase));
            float starPhase = std::fmod(globalPhase + s.phase * 0.159f, 1.0f);
            float baseX = s.x - 0.5f;
            float baseY = s.y - 0.5f;
            float expand = 0.3f + starPhase * 0.7f;
            float sx = cx + baseX * expand * w;
            float sy = cy + baseY * expand * h;
            float sizeMult = 0.8f + starPhase * 0.7f;
            float drawSize = s.size * sizeMult;
            float fadeMult = std::sin(starPhase * juce::MathConstants<float>::pi);
            g.setColour(s.colour.withAlpha(twinkle * fadeMult));
            g.fillEllipse(sx, sy, drawSize, drawSize);
        }
    }

    // Crosshairs
    {
        float dashLengths[] = { 2.0f, 6.0f };

        juce::Path hSrc, hDashed;
        hSrc.startNewSubPath(cx - radius, cy);
        hSrc.lineTo(cx + radius, cy);
        juce::PathStrokeType(0.5f).createDashedStroke(hDashed, hSrc, dashLengths, 2);
        g.setColour(MapColours::textEtched.withAlpha(0.6f));
        g.fillPath(hDashed);

        juce::Path vSrc, vDashed;
        vSrc.startNewSubPath(cx, cy - radius);
        vSrc.lineTo(cx, cy + radius);
        juce::PathStrokeType(0.5f).createDashedStroke(vDashed, vSrc, dashLengths, 2);
        g.fillPath(vDashed);

        float diagDash[] = { 1.0f, 8.0f };
        float diagR = radius * 0.707f;
        g.setColour(MapColours::textEtched.withAlpha(0.12f));

        juce::Path d1Src, d1Dashed;
        d1Src.startNewSubPath(cx - diagR, cy - diagR);
        d1Src.lineTo(cx + diagR, cy + diagR);
        juce::PathStrokeType(0.25f).createDashedStroke(d1Dashed, d1Src, diagDash, 2);
        g.fillPath(d1Dashed);

        juce::Path d2Src, d2Dashed;
        d2Src.startNewSubPath(cx + diagR, cy - diagR);
        d2Src.lineTo(cx - diagR, cy + diagR);
        juce::PathStrokeType(0.25f).createDashedStroke(d2Dashed, d2Src, diagDash, 2);
        g.fillPath(d2Dashed);
    }

    // Distance rings
    for (float r = 0.25f; r <= 1.0f; r += 0.25f)
    {
        float ringR = r * radius;
        g.setColour(MapColours::textEtched.withAlpha(0.6f));
        g.drawEllipse(cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, 0.7f);
    }

    // Ring labels
    g.setColour(MapColours::textEtched.withAlpha(0.45f));
    if (lf && lf->jetbrainsRegular)
        g.setFont(juce::Font(juce::FontOptions(lf->jetbrainsRegular).withHeight(9.0f)));
    else
        g.setFont(juce::FontOptions(9.0f));
    for (int ri = 1; ri <= 4; ++ri)
    {
        float r = ri * 0.25f;
        float ringR = r * radius;

        float lrAngle = juce::MathConstants<float>::pi / 6.0f;
        float lx = cx + std::cos(lrAngle) * ringR + 5.0f;
        float ly = cy + std::sin(lrAngle) * ringR + 4.0f;
        g.drawText(juce::String(r, 2), juce::roundToInt(lx), juce::roundToInt(ly), 28, 10,
                    juce::Justification::centredLeft);

        float ulAngle = juce::MathConstants<float>::pi * 7.0f / 6.0f;
        float mx = cx + std::cos(ulAngle) * ringR - 30.0f;
        float my = cy + std::sin(ulAngle) * ringR - 12.0f;
        int meters = juce::roundToInt(r * r * 20.0f);
        g.drawText(juce::String(meters) + "m", juce::roundToInt(mx), juce::roundToInt(my), 28, 10,
                    juce::Justification::centredRight);
    }

    // Center reticle
    {
        float reticleR = 7.0f;
        g.setColour(MapColours::textEtched.withAlpha(0.6f));
        g.drawEllipse(cx - reticleR, cy - reticleR, reticleR * 2.0f, reticleR * 2.0f, 0.7f);
        g.fillEllipse(cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
        float armLen = 4.0f;
        g.drawLine(cx - reticleR - armLen, cy, cx - reticleR, cy, 0.5f);
        g.drawLine(cx + reticleR, cy, cx + reticleR + armLen, cy, 0.5f);
        g.drawLine(cx, cy - reticleR - armLen, cx, cy - reticleR, 0.5f);
        g.drawLine(cx, cy + reticleR, cx, cy + reticleR + armLen, 0.5f);
    }

    // Cardinals
    g.setColour(MapColours::textEtched);
    if (lf && lf->jetbrainsMedium)
        g.setFont(juce::Font(juce::FontOptions(lf->jetbrainsMedium).withHeight(14.0f).withKerningFactor(0.2f)));
    else
        g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("F",  juce::Rectangle<float>(cx - 30.0f, cy - radius - 20.0f, 60.0f, 16.0f), juce::Justification::centred);
    g.drawText("B",  juce::Rectangle<float>(cx - 30.0f, cy + radius + 4.0f,  60.0f, 16.0f), juce::Justification::centred);
    g.drawText("L",  juce::roundToInt(cx - radius - 22), juce::roundToInt(cy - 8),  24, 16, juce::Justification::centred);
    g.drawText("R",  juce::roundToInt(cx + radius + 2),  juce::roundToInt(cy - 8),  24, 16, juce::Justification::centred);

    // Trajectory glow trail for selected object
    if (selectedObject >= 0 && selectedObject < MAX_SOURCES
        && objects[static_cast<size_t>(selectedObject)].enabled
        && trajectoryStates[static_cast<size_t>(selectedObject)].shape != 0)
    {
        auto& ts = trajectoryStates[static_cast<size_t>(selectedObject)];
        auto objCol = objectColours[selectedObject];

        bool drawTrail = (ts.shape != 10);
        constexpr int kPathSamples = 240;
        struct PathPoint { juce::Point<float> px; float elDeg; float phase; };
        PathPoint pathPoints[kPathSamples];

        if (drawTrail)
        {
            for (int s = 0; s < kPathSamples; ++s)
            {
                float samplePhase = (float)s / (float)kPathSamples;
                auto result = TrajectoryEngine::compute(
                    static_cast<TrajectoryShape>(ts.shape), samplePhase,
                    ts.originAzDeg, ts.originElDeg, ts.originDist, ts.reverse);
                pathPoints[s].px    = spatialToPixel(result.azDeg, result.dist);
                pathPoints[s].elDeg = result.elDeg;
                pathPoints[s].phase = samplePhase;
            }

            for (int s = 0; s < kPathSamples; ++s)
            {
                int next = (s + 1) % kPathSamples;
                auto& p0 = pathPoints[s];
                auto& p1 = pathPoints[next];

                if (p0.px.getDistanceFrom(p1.px) > radius * 0.8f)
                    continue;
                if (ts.shape == 11 && next == 0)
                    continue;

                float phaseDist = std::abs(p0.phase - ts.phase);
                if (phaseDist > 0.5f) phaseDist = 1.0f - phaseDist;
                float proximity = 1.0f - (phaseDist * 6.0f);
                proximity = juce::jlimit(0.0f, 1.0f, proximity);
                float glowAlpha = 0.05f + proximity * 0.55f;

                float avgEl = (p0.elDeg + p1.elDeg) * 0.5f;
                float elNorm = (avgEl + 90.0f) / 180.0f;
                float elOpacity = 0.3f + elNorm * 0.7f;
                float thickness = 1.0f + elNorm * 4.5f;

                float finalAlpha = glowAlpha * elOpacity;
                g.setColour(objCol.withAlpha(finalAlpha));
                g.drawLine(p0.px.x, p0.px.y, p1.px.x, p1.px.y, thickness);
            }
        }

        // Origin marker
        auto originPx = spatialToPixel(ts.originAzDeg, ts.originDist);
        originPx = { std::round(originPx.x), std::round(originPx.y) };
        {
            float armLen = 8.0f;
            g.setColour(objCol.withAlpha(0.5f));
            g.drawLine(originPx.x - armLen, originPx.y, originPx.x + armLen, originPx.y, 1.2f);
            g.drawLine(originPx.x, originPx.y - armLen, originPx.x, originPx.y + armLen, 1.2f);
        }
    }

    // Draw objects (selected on top)
    int drawOrder[MAX_SOURCES];
    int drawCount = 0;
    for (int i = 0; i < MAX_SOURCES; ++i)
        if (i != selectedObject) drawOrder[drawCount++] = i;
    if (selectedObject >= 0 && selectedObject < MAX_SOURCES)
        drawOrder[drawCount++] = selectedObject;

    for (int di = 0; di < drawCount; ++di)
    {
        int i = drawOrder[di];
        if (! objects[static_cast<size_t>(i)].enabled) continue;

        auto pos = spatialToPixel(objects[static_cast<size_t>(i)].azimuthDeg,
                                  objects[static_cast<size_t>(i)].distance);
        pos = { std::round(pos.x), std::round(pos.y) };

        float elDeg = objects[static_cast<size_t>(i)].elevationDeg;
        bool isAbove = (elDeg >= 0.0f);
        float z = std::sin(juce::degreesToRadians(elDeg));

        float baseDiam = (i == selectedObject) ? 18.0f : 14.0f;
        float elevScale = (z >= 0.0f) ? 5.0f : 3.0f;
        float dotSize = baseDiam + elevScale * z;
        float half = dotSize * 0.5f;

        // Selection halo
        if (i == selectedObject)
        {
            g.setColour(objectColours[i].withAlpha(isAbove ? 0.3f : 0.15f));
            float haloSize = dotSize + 8.0f;
            float haloHalf = haloSize * 0.5f;
            g.fillEllipse(pos.x - haloHalf, pos.y - haloHalf, haloSize, haloSize);
        }

        // Activity glow
        if (activityLevel[static_cast<size_t>(i)] > 0.01f)
        {
            float act = juce::jlimit(0.0f, 1.0f, activityLevel[static_cast<size_t>(i)]);
            float phase = pulsePhase[static_cast<size_t>(i)];

            float ambientR = half + 8.0f + act * 6.0f;
            g.setColour(objectColours[i].withAlpha(act * 0.20f));
            g.fillEllipse(pos.x - ambientR, pos.y - ambientR, ambientR * 2.0f, ambientR * 2.0f);

            float ringOpacity = 0.0f;
            float ringScale = 1.0f;
            if (phase < 0.03f)
            {
                float t = phase / 0.03f;
                ringOpacity = t * 0.7f;
                ringScale = 1.0f;
            }
            else if (phase < 0.08f)
            {
                float t = (phase - 0.03f) / 0.05f;
                ringOpacity = 0.7f - t * 0.1f;
                ringScale = 1.0f + t * 0.3f;
            }
            else if (phase < 0.20f)
            {
                float t = (phase - 0.08f) / 0.12f;
                ringOpacity = 0.6f * (1.0f - t);
                ringScale = 1.3f + t * 0.5f;
            }

            if (ringOpacity > 0.01f)
            {
                float ringR = (half + 4.0f) * ringScale;
                g.setColour(objectColours[i].withAlpha(ringOpacity * act));
                g.drawEllipse(pos.x - ringR, pos.y - ringR, ringR * 2.0f, ringR * 2.0f, 1.5f);
            }
        }

        // Dot outline
        juce::Path dotPath;
        dotPath.addEllipse(pos.x - half, pos.y - half, dotSize, dotSize);

        float coreBrightness = 1.0f;
        if (activityLevel[static_cast<size_t>(i)] > 0.01f)
        {
            float phase = pulsePhase[static_cast<size_t>(i)];
            if (phase < 0.03f)
                coreBrightness = 1.0f + (phase / 0.03f) * 0.5f;
            else if (phase < 0.08f)
                coreBrightness = 1.5f - ((phase - 0.03f) / 0.05f) * 0.1f;
            else if (phase < 0.18f)
                coreBrightness = 1.4f - ((phase - 0.08f) / 0.10f) * 0.4f;
        }
        auto coreColour = (coreBrightness > 1.01f)
                        ? objectColours[i].brighter(coreBrightness - 1.0f)
                        : objectColours[i];

        g.setColour(coreColour);
        g.strokePath(dotPath, juce::PathStrokeType(1.2f));

        g.setColour(coreColour.withAlpha(isAbove ? 1.0f : 0.3f));
        g.fillPath(dotPath);

        // Number label
        {
            auto labelColour = isAbove ? juce::Colour(0xff161820) : objectColours[i];
            juce::Font labelFont(juce::FontOptions(10.0f));
            if (lf && lf->jetbrainsBold)
                labelFont = juce::Font(juce::FontOptions(lf->jetbrainsBold).withHeight(10.0f));
            juce::GlyphArrangement glyphs;
            juce::String numText(i + 1);
            glyphs.addLineOfText(labelFont, numText, 0.0f, 0.0f);
            auto glyphBounds = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true);
            float gx = pos.x - glyphBounds.getWidth() * 0.5f - glyphBounds.getX();
            float gy = pos.y - glyphBounds.getHeight() * 0.5f - glyphBounds.getY();
            glyphs.moveRangeOfGlyphs(0, -1, gx, gy);

            juce::Path textPath;
            glyphs.createPath(textPath);
            g.setColour(labelColour);
            g.fillPath(textPath);
            g.strokePath(textPath, juce::PathStrokeType(0.8f));
        }

        // Elevation label (selected only)
        if (i == selectedObject && std::abs(elDeg) > 1.0f)
        {
            float labelOffsetY = isAbove ? -(half + 14.0f) : (half + 2.0f);
            g.setColour(objectColours[i].withAlpha(0.85f));
            if (lf && lf->jetbrainsRegular)
                g.setFont(juce::Font(juce::FontOptions(lf->jetbrainsRegular).withHeight(11.0f)));
            else
                g.setFont(juce::FontOptions(11.0f));
            juce::String elText = (elDeg > 0.0f ? "+" : "")
                                + juce::String(juce::roundToInt(elDeg))
                                + juce::String::charToString(0x00B0);
            g.drawText(elText, juce::roundToInt(pos.x - 18), juce::roundToInt(pos.y + labelOffsetY),
                        36, 12, juce::Justification::centred);
        }

        // OSC override label
        if (oscOverride[static_cast<size_t>(i)])
        {
            float oscLabelY = (i == selectedObject && elDeg < -1.0f)
                            ? pos.y + half + 14.0f
                            : pos.y + half + 1.0f;
            g.setColour(MapColours::accentStellar);
            if (lf && lf->jetbrainsBold)
                g.setFont(juce::Font(juce::FontOptions(lf->jetbrainsBold).withHeight(10.0f)));
            else
                g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText("OSC", juce::roundToInt(pos.x - half - 2), juce::roundToInt(oscLabelY),
                        juce::roundToInt(dotSize + 4), 10, juce::Justification::centred);
        }
    }
}

void SpatialMapComponent::mouseDown(const juce::MouseEvent& e)
{
    draggedObject = findObjectAt(e.position);
    if (draggedObject >= 0)
        listenerList.call([this](Listener& l) { l.objectSelected(draggedObject); });
}

void SpatialMapComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedObject < 0) return;

    auto result = pixelToSpatial(e.position);
    float azDeg = result.first;
    float dist  = result.second;
    objects[static_cast<size_t>(draggedObject)].azimuthDeg = azDeg;
    objects[static_cast<size_t>(draggedObject)].distance   = dist;
    repaint();

    listenerList.call([this, azDeg, dist](Listener& l) {
        l.objectPositionChanged(draggedObject, azDeg, dist);
    });
}

} // namespace spatialcore
