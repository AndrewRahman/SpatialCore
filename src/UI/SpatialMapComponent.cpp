#include <SpatialCore/UI/SpatialMapComponent.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <cmath>

namespace spatialcore
{

namespace
{
    // Colours matching OSD's Colours_OSD namespace (Source/PluginEditor.cpp)
    const juce::Colour mapVoid       (0xff010204);  // oklch(8% 0.015 260)
    const juce::Colour textEtched    (0xff5a5e63);  // oklch(48% 0.01 260) — cardinals
    const juce::Colour accentStellar (0xff80d8ff);  // oklch(85% 0.12 240) — delay section

    // Helper: create a Font from a specific Typeface with height and optional kerning
    juce::Font makeFont (juce::Typeface::Ptr tf, float height, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (tf).withHeight (height).withKerningFactor (kerning));
    }
}

const juce::Colour SpatialMapComponent::objectColours[SpatialMapComponent::MAX_SOURCES] = {
    juce::Colour (0xffED5E5E),  //  1 red        — hsl(0, 80%, 65%)
    juce::Colour (0xffEDA65E),  //  2 orange     — hsl(30, 80%, 65%)
    juce::Colour (0xffACD435),  //  3 lime       — hsl(75, 65%, 52%)
    juce::Colour (0xff3BCE6C),  //  4 green      — hsl(140, 60%, 52%)
    juce::Colour (0xff3CDDA7),  //  5 teal       — hsl(160, 70%, 55%)
    juce::Colour (0xff52E0E0),  //  6 cyan       — hsl(180, 70%, 60%)
    juce::Colour (0xff4DB3E6),  //  7 light blue — hsl(200, 75%, 60%)
    juce::Colour (0xff6390E9),  //  8 blue       — hsl(220, 75%, 65%)
    juce::Colour (0xff6767E4),  //  9 indigo     — hsl(240, 70%, 65%)
    juce::Colour (0xffA667E4),  // 10 violet     — hsl(270, 70%, 65%)
    juce::Colour (0xffE467E4),  // 11 magenta    — hsl(300, 70%, 65%)
    juce::Colour (0xffE963A6),  // 12 rose       — hsl(330, 75%, 65%)
};

SpatialMapComponent::SpatialMapComponent (int numObjects)
    : numObjects_ (juce::jlimit (0, MAX_SOURCES, numObjects))
{
    generateStars();
}

void SpatialMapComponent::generateStars()
{
    // ~80 stars with deterministic LCG random positions (increased from 50 for visibility)
    stars.clear();
    uint32_t seed = 0x5ACEA10u;  // deterministic seed
    auto lcg = [&seed]() -> float {
        seed = seed * 1103515245u + 12345u;
        return (float) ((seed >> 16) & 0x7FFF) / 32767.0f;
    };

    // Star colours from prototype: 82% cyan-white, 15% amber, 3% violet
    auto cyanWhite = juce::Colour (0xffe0f0fa);   // oklch(90% 0.02 240)
    auto amber     = juce::Colour (0xfff0d8a8);   // oklch(85% 0.05 70)
    auto violet    = juce::Colour (0xffc0a8f0);   // oklch(80% 0.06 290)

    for (int i = 0; i < 80; ++i)
    {
        Star s;
        s.x = lcg();
        s.y = lcg();
        s.phase = lcg() * juce::MathConstants<float>::twoPi;
        s.speed = 0.7f + lcg() * 1.4f;  // 3-9s cycle (2π/2.1 ≈ 3s, 2π/0.7 ≈ 9s)
        s.size = lcg() < 0.80f ? 1.0f : 2.0f;  // 20% are 2px (increased from 12%)

        float colRoll = lcg();
        if (colRoll < 0.82f)       s.colour = cyanWhite;
        else if (colRoll < 0.97f)  s.colour = amber;
        else                       s.colour = violet;

        stars.push_back (s);
    }
}

juce::Point<float> SpatialMapComponent::spatialToPixel (float azimuthDeg, float dist) const
{
    float radius = std::min (getWidth(), getHeight()) * 0.45f;
    float cx = getWidth()  * 0.5f;
    float cy = getHeight() * 0.5f;

    float azRad = juce::degreesToRadians (azimuthDeg);
    float x = cx - std::sin (azRad) * dist * radius;
    float y = cy - std::cos (azRad) * dist * radius;
    return { x, y };
}

std::pair<float, float> SpatialMapComponent::pixelToSpatial (juce::Point<float> pixel) const
{
    float radius = std::min (getWidth(), getHeight()) * 0.45f;
    float cx = getWidth()  * 0.5f;
    float cy = getHeight() * 0.5f;

    float dx = (pixel.x - cx) / radius;
    float dy = (pixel.y - cy) / radius;
    float dist = std::sqrt (dx * dx + dy * dy);
    dist = juce::jlimit (0.0f, 1.0f, dist);

    float azRad = std::atan2 (-dx, -dy);
    float azDeg = juce::radiansToDegrees (azRad);

    return { azDeg, dist };
}

int SpatialMapComponent::findObjectAt (juce::Point<float> pos) const
{
    // Issue #98: Check selected tap first (it renders on top)
    auto hitTest = [&](int i) -> bool
    {
        if (! objects[(size_t)i].enabled) return false;
        auto p = spatialToPixel (objects[(size_t)i].azimuthDeg, objects[(size_t)i].distance);

        // Variable hit radius based on elevation-dependent dot size
        float elDeg = objects[(size_t)i].elevationDeg;
        float z = std::sin (juce::degreesToRadians (elDeg));
        float baseDiam = (i == selectedObject) ? 18.0f : 14.0f;
        float elevScale = (z >= 0.0f) ? 5.0f : 3.0f;  // asymmetric: bigger up, gentler down
        float currentDotSize = baseDiam + elevScale * z;
        float hitRadius = std::max (currentDotSize * 0.5f + 2.0f, 10.0f);

        return p.getDistanceFrom (pos) < hitRadius;
    };

    // Selected tap is visually on top, so check it first
    if (selectedObject >= 0 && selectedObject < numObjects_ && hitTest (selectedObject))
        return selectedObject;

    for (int i = numObjects_ - 1; i >= 0; --i)
    {
        if (i == selectedObject) continue;
        if (hitTest (i))
            return i;
    }
    return -1;
}

void SpatialMapComponent::setObjectState (int index, float azimuthDeg, float elevationDeg,
                                        float distance, bool enabled)
{
    if (index < 0 || index >= numObjects_) return;
    objects[(size_t)index].azimuthDeg   = azimuthDeg;
    objects[(size_t)index].elevationDeg = elevationDeg;
    objects[(size_t)index].distance     = distance;
    objects[(size_t)index].enabled      = enabled;
    repaint();
}

void SpatialMapComponent::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<SMLLookAndFeel*> (&getLookAndFeel());
    float radius = std::min (getWidth(), getHeight()) * 0.45f;
    float cx = getWidth()  * 0.5f;
    float cy = getHeight() * 0.5f;

    // Observatory v6: nearly pure black void
    g.fillAll (mapVoid);

    // Subtle radial vignette
    {
        juce::ColourGradient vignette (mapVoid.withAlpha (0.0f), cx, cy,
                                       juce::Colour (0xff000000).withAlpha (0.5f),
                                       cx + (float) getWidth() * 0.5f, cy, true);
        g.setGradientFill (vignette);
        g.fillRect (getLocalBounds());
    }

    // Star field — twinkling dots with forward parallax (expanding from center)
    {
        float w = (float) getWidth();
        float h = (float) getHeight();
        float globalPhase = std::fmod (starTime * 0.04f, 1.0f);  // continuous 0→1 cycle, ~25s

        for (auto& s : stars)
        {
            float twinkle = 0.15f + 0.50f * (0.5f + 0.5f * std::sin (starTime * s.speed + s.phase));

            // Per-star phase offset for depth staggering
            float starPhase = std::fmod (globalPhase + s.phase * 0.159f, 1.0f);  // 0.159 ≈ 1/2π

            // Expand from center: at phase≈0 star is near center, at phase≈1 near edge
            float baseX = s.x - 0.5f;   // -0.5..+0.5 relative to center
            float baseY = s.y - 0.5f;
            float expand = 0.3f + starPhase * 0.7f;  // scale 0.3→1.0

            float sx = cx + baseX * expand * w;
            float sy = cy + baseY * expand * h;

            // Grow slightly as they approach (0.8x → 1.5x)
            float sizeMult = 0.8f + starPhase * 0.7f;
            float drawSize = s.size * sizeMult;

            // Fade: appear faint near center, brighten mid-distance, fade at edge
            float fadeMult = std::sin (starPhase * juce::MathConstants<float>::pi);
            g.setColour (s.colour.withAlpha (twinkle * fadeMult));

            g.fillEllipse (sx, sy, drawSize, drawSize);
        }
    }

    // Crosshairs — subtle dashed lines
    {
        float dashLengths[] = { 2.0f, 6.0f };

        // Horizontal dashed line
        juce::Path hSrc, hDashed;
        hSrc.startNewSubPath (cx - radius, cy);
        hSrc.lineTo (cx + radius, cy);
        juce::PathStrokeType (0.5f).createDashedStroke (hDashed, hSrc, dashLengths, 2);
        g.setColour (textEtched.withAlpha (0.6f));
        g.fillPath (hDashed);

        // Vertical dashed line
        juce::Path vSrc, vDashed;
        vSrc.startNewSubPath (cx, cy - radius);
        vSrc.lineTo (cx, cy + radius);
        juce::PathStrokeType (0.5f).createDashedStroke (vDashed, vSrc, dashLengths, 2);
        g.fillPath (vDashed);

        // Diagonal crosshairs (±45°, subtler)
        float diagDash[] = { 1.0f, 8.0f };
        float diagR = radius * 0.707f;
        g.setColour (textEtched.withAlpha (0.12f));

        juce::Path d1Src, d1Dashed;
        d1Src.startNewSubPath (cx - diagR, cy - diagR);
        d1Src.lineTo (cx + diagR, cy + diagR);
        juce::PathStrokeType (0.25f).createDashedStroke (d1Dashed, d1Src, diagDash, 2);
        g.fillPath (d1Dashed);

        juce::Path d2Src, d2Dashed;
        d2Src.startNewSubPath (cx + diagR, cy - diagR);
        d2Src.lineTo (cx - diagR, cy + diagR);
        juce::PathStrokeType (0.25f).createDashedStroke (d2Dashed, d2Src, diagDash, 2);
        g.fillPath (d2Dashed);
    }

    // Distance rings — thin stroke matching center reticle weight
    for (float r = 0.25f; r <= 1.0f; r += 0.25f)
    {
        float ringR = r * radius;
        g.setColour (textEtched.withAlpha (0.6f));
        g.drawEllipse (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, 0.7f);
    }

    // Ring labels: distance values at 30° (lower-right), meter values at 210° (upper-left)
    g.setColour (textEtched.withAlpha (0.45f));
    if (lf) g.setFont (makeFont (lf->jetbrainsRegular, 9.0f));
    else    g.setFont (juce::FontOptions (9.0f));
    for (int ri = 1; ri <= 4; ++ri)
    {
        float r = ri * 0.25f;
        float ringR = r * radius;

        // Distance values at 30° (lower-right from center)
        float lrAngle = juce::MathConstants<float>::pi / 6.0f;  // 30° in screen coords
        float lx = cx + std::cos (lrAngle) * ringR + 5.0f;
        float ly = cy + std::sin (lrAngle) * ringR + 4.0f;
        g.drawText (juce::String (r, 2), juce::roundToInt (lx), juce::roundToInt (ly), 28, 10,
                    juce::Justification::centredLeft);

        // Meter values at 210° (upper-left from center)
        float ulAngle = juce::MathConstants<float>::pi * 7.0f / 6.0f;  // 210° in screen coords
        float mx = cx + std::cos (ulAngle) * ringR - 30.0f;
        float my = cy + std::sin (ulAngle) * ringR - 12.0f;
        int meters = juce::roundToInt (r * r * 20.0f);
        g.drawText (juce::String (meters) + "m", juce::roundToInt (mx), juce::roundToInt (my), 28, 10,
                    juce::Justification::centredRight);
    }

    // Center reticle (ring + dot + short cross arms)
    {
        float reticleR = 7.0f;
        // Reticle ring
        g.setColour (textEtched.withAlpha (0.6f));
        g.drawEllipse (cx - reticleR, cy - reticleR, reticleR * 2.0f, reticleR * 2.0f, 0.7f);
        // Center dot
        g.setColour (textEtched.withAlpha (0.6f));
        g.fillEllipse (cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
        // Short cross arms (4px each side)
        g.setColour (textEtched.withAlpha (0.6f));
        float armLen = 4.0f;
        g.drawLine (cx - reticleR - armLen, cy, cx - reticleR, cy, 0.5f);
        g.drawLine (cx + reticleR, cy, cx + reticleR + armLen, cy, 0.5f);
        g.drawLine (cx, cy - reticleR - armLen, cx, cy - reticleR, 0.5f);
        g.drawLine (cx, cy + reticleR, cx, cy + reticleR + armLen, 0.5f);
    }

    // Cardinals — JetBrains Mono Medium, wide tracking, larger text
    g.setColour (textEtched);
    if (lf) g.setFont (makeFont (lf->jetbrainsMedium, 14.0f, 0.2f));
    else    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("F",  juce::Rectangle<float> (cx - 30.0f, cy - radius - 20.0f, 60.0f, 16.0f), juce::Justification::centred);
    g.drawText ("B",  juce::Rectangle<float> (cx - 30.0f, cy + radius + 4.0f,  60.0f, 16.0f), juce::Justification::centred);
    g.drawText ("L",  juce::roundToInt (cx - radius - 22), juce::roundToInt (cy - 8),  24, 16, juce::Justification::centred);
    g.drawText ("R",  juce::roundToInt (cx + radius + 2),  juce::roundToInt (cy - 8),  24, 16, juce::Justification::centred);

    // =========================================================================
    // v0.9 PROTOTYPE: Glow trail + origin markers for active trajectories
    // Renders behind object dots. Only drawn for selected tap with active trajectory.
    // =========================================================================
    if (selectedObject >= 0 && selectedObject < numObjects_
        && objects[(size_t)selectedObject].enabled
        && trajectoryStates[(size_t)selectedObject].shape != 0)
    {
        auto& ts = trajectoryStates[(size_t)selectedObject];
        auto objCol = objectColours[selectedObject];

        // --- Glow trail: sample trajectory path at ~240 phase points ---
        // Higher sample count for smooth trails with large origin-relative shapes
        // Skip for Random (shape 10) — path is non-deterministic, can't be pre-sampled
        bool drawTrail = (ts.shape != 10);
        constexpr int kPathSamples = 240;
        struct PathPoint { juce::Point<float> px; float elDeg; float phase; };
        PathPoint pathPoints[kPathSamples];

        if (drawTrail)
        {
            for (int s = 0; s < kPathSamples; ++s)
            {
                float samplePhase = (float) s / (float) kPathSamples;
                auto result = TrajectoryEngine::computeTrajectory (
                    ts.shape, samplePhase, ts.originAzDeg, ts.originElDeg, ts.originDist, ts.reverse);
                pathPoints[s].px    = spatialToPixel (result.azDeg, result.dist);
                pathPoints[s].elDeg = result.elDeg;
                pathPoints[s].phase = samplePhase;
            }

            // Draw glow trail segments
            for (int s = 0; s < kPathSamples; ++s)
            {
                int next = (s + 1) % kPathSamples;
                auto& p0 = pathPoints[s];
                auto& p1 = pathPoints[next];

                // Skip segments that wrap across the map (large pixel jumps)
                if (p0.px.getDistanceFrom (p1.px) > radius * 0.8f)
                    continue;

                // Spiral: skip the wrap-back segment from end (outer edge) to start (center)
                if (ts.shape == 11 && next == 0)
                    continue;

                // Brightness: proximity to current animated dot position
                // Tighter focus (×6) for concentrated glow near the moving dot.
                // Issue #168 r3: in hero-screenshot mode, keep the proximity
                // fade so the animated dot still has a bright focus, but
                // lift the baseline so the rest of the path reads as a faint
                // continuous curve rather than a near-invisible 0.05-alpha
                // outline. Peak brightness stays the same.
                float phaseDist = std::abs (p0.phase - ts.phase);
                if (phaseDist > 0.5f) phaseDist = 1.0f - phaseDist;
                float proximity = 1.0f - (phaseDist * 6.0f);
                proximity = juce::jlimit (0.0f, 1.0f, proximity);
                float glowAlpha = drawFullTrajectoryForScreenshot
                                ? (0.18f + proximity * 0.42f)
                                : (0.05f + proximity * 0.55f);

                // Elevation encoding: opacity + thickness
                float avgEl = (p0.elDeg + p1.elDeg) * 0.5f;
                float elNorm = (avgEl + 90.0f) / 180.0f;
                float elOpacity = 0.3f + elNorm * 0.7f;
                float thickness = 1.0f + elNorm * 4.5f;

                float finalAlpha = glowAlpha * elOpacity;
                g.setColour (objCol.withAlpha (finalAlpha));
                g.drawLine (p0.px.x, p0.px.y, p1.px.x, p1.px.y, thickness);
            }
        }
        else if (ts.shape == 10 && trajectoryEngine != nullptr)
        {
            // Symmetric time-windowed trail: look-back + look-ahead centered on randomTime
            // Unlike deterministic shapes (which outline a compact closed loop at 0.05 base alpha),
            // Random's sprawling path needs zero base alpha — only the proximity glow near the dot.
            // In screenshot mode (drawFullTrajectoryForScreenshot), widen the
            // window and flatten the decay so the still frame matches the
            // visual weight of the deterministic shapes.
            constexpr int kRandomSamples = 240;
            const float kHalfWindow = drawFullTrajectoryForScreenshot ? 1.0f : 1.0f;
            const float kDecayRate  = drawFullTrajectoryForScreenshot ? 0.0f : 3.0f;
            const float kBaseAlpha  = drawFullTrajectoryForScreenshot ? 0.25f : 0.0f;
            const float kPeakAlpha  = drawFullTrajectoryForScreenshot ? 0.35f : 0.60f;

            struct RndPathPoint { juce::Point<float> px; float elDeg; float timeOffset; };
            RndPathPoint rndPath[kRandomSamples];

            for (int s = 0; s < kRandomSamples; ++s)
            {
                float timeOffset = -kHalfWindow + (float) s / (float) (kRandomSamples - 1) * (2.0f * kHalfWindow);
                float sampleTime = ts.randomTime + timeOffset;
                auto rp = trajectoryEngine->evaluateRandomNoise (selectedObject, sampleTime);
                float az   = ts.originAzDeg + rp.azDeg;
                float el   = juce::jlimit (-90.0f, 90.0f, ts.originElDeg + rp.elDeg);
                float distScaleR = 1.0f - ts.originDist;  // match tick() distance scaling
                float dist = juce::jlimit (0.0f, 1.0f, ts.originDist + rp.dist * distScaleR);
                az = wrapAzimuth (az);

                rndPath[s].px         = spatialToPixel (az, dist);
                rndPath[s].elDeg      = el;
                rndPath[s].timeOffset = timeOffset;
            }

            for (int s = 0; s < kRandomSamples - 1; ++s)
            {
                auto& p0 = rndPath[s];
                auto& p1 = rndPath[s + 1];

                // Skip segments that wrap across the map
                if (p0.px.getDistanceFrom (p1.px) > radius * 0.8f)
                    continue;

                // Brightness: proximity to current time (timeOffset == 0)
                float proximity = 1.0f - std::abs (p0.timeOffset) * kDecayRate;
                proximity = juce::jlimit (0.0f, 1.0f, proximity);
                float glowAlpha = kBaseAlpha + proximity * kPeakAlpha;

                // Elevation encoding: opacity + thickness (same as other shapes)
                float avgEl = (p0.elDeg + p1.elDeg) * 0.5f;
                float elNorm = (avgEl + 90.0f) / 180.0f;
                float elOpacity = 0.3f + elNorm * 0.7f;
                float thickness = 1.0f + elNorm * 4.5f;

                float finalAlpha = glowAlpha * elOpacity;
                g.setColour (objCol.withAlpha (finalAlpha));
                g.drawLine (p0.px.x, p0.px.y, p1.px.x, p1.px.y, thickness);
            }
        }

        // --- Origin marker: crosshair at captured base position ---
        auto originPx = spatialToPixel (ts.originAzDeg, ts.originDist);
        originPx = { std::round (originPx.x), std::round (originPx.y) };
        {
            float armLen = 8.0f;
            g.setColour (objCol.withAlpha (0.5f));
            g.drawLine (originPx.x - armLen, originPx.y, originPx.x + armLen, originPx.y, 1.2f);
            g.drawLine (originPx.x, originPx.y - armLen, originPx.x, originPx.y + armLen, 1.2f);
        }
    }

    // Issue #98: Build draw order so selected tap renders on top
    std::vector<int> drawOrder;
    drawOrder.reserve ((size_t) numObjects_);
    for (int i = 0; i < numObjects_; ++i)
        if (i != selectedObject) drawOrder.push_back (i);
    if (selectedObject >= 0 && selectedObject < numObjects_)
        drawOrder.push_back (selectedObject);

    for (int i : drawOrder)
    {
        if (! objects[(size_t)i].enabled) continue;

        auto pos = spatialToPixel (objects[(size_t)i].azimuthDeg, objects[(size_t)i].distance);
        pos = { std::round (pos.x), std::round (pos.y) };  // pixel-grid snap for HiDPI sharpness

        // v0.6: IEM-faithful elevation visualization
        // Elevation encoded through dot visual properties: size, opacity, outline, text
        // (no stems — matches IEM StereoEncoder / Nuendo / Pro Tools industry standard)
        float elDeg = objects[(size_t)i].elevationDeg;
        bool isAbove = (elDeg >= 0.0f);
        float z = std::sin (juce::degreesToRadians (elDeg));  // -1..+1

        float baseDiam = (i == selectedObject) ? 18.0f : 14.0f;
        float elevScale = (z >= 0.0f) ? 5.0f : 3.0f;  // asymmetric: +5px up, -3px down
        float dotSize = baseDiam + elevScale * z;
        float half = dotSize * 0.5f;

        // 1. Selection halo (scales with dot, alpha adapts by hemisphere)
        if (i == selectedObject)
        {
            g.setColour (objectColours[i].withAlpha (isAbove ? 0.3f : 0.15f));
            float haloSize = dotSize + 8.0f;
            float haloHalf = haloSize * 0.5f;
            g.fillEllipse (pos.x - haloHalf, pos.y - haloHalf, haloSize, haloSize);
        }

        // 1b. Activity glow — expanding pulse ring + core glow (v0.7)
        // Matches prototype tapPulse: ring expands scale(1)→scale(1.8), fades out
        if (activityLevel[(size_t)i] > 0.01f)
        {
            float act = juce::jlimit (0.0f, 1.0f, activityLevel[(size_t)i]);
            float phase = pulsePhase[(size_t)i];

            // Soft ambient glow (always present when active)
            float ambientR = half + 8.0f + act * 6.0f;
            g.setColour (objectColours[i].withAlpha (act * 0.20f));
            g.fillEllipse (pos.x - ambientR, pos.y - ambientR, ambientR * 2.0f, ambientR * 2.0f);

            // Expanding pulse ring (prototype tapPulse keyframes)
            float ringOpacity = 0.0f;
            float ringScale = 1.0f;
            if (phase < 0.03f)
            {
                // 0%→3%: opacity 0→0.7, scale 1.0
                float t = phase / 0.03f;
                ringOpacity = t * 0.7f;
                ringScale = 1.0f;
            }
            else if (phase < 0.08f)
            {
                // 3%→8%: opacity 0.7→0.6, scale 1.0→1.3
                float t = (phase - 0.03f) / 0.05f;
                ringOpacity = 0.7f - t * 0.1f;
                ringScale = 1.0f + t * 0.3f;
            }
            else if (phase < 0.20f)
            {
                // 8%→20%: opacity 0.6→0, scale 1.3→1.8
                float t = (phase - 0.08f) / 0.12f;
                ringOpacity = 0.6f * (1.0f - t);
                ringScale = 1.3f + t * 0.5f;
            }
            // 20%→100%: invisible

            if (ringOpacity > 0.01f)
            {
                float ringR = (half + 4.0f) * ringScale;
                g.setColour (objectColours[i].withAlpha (ringOpacity * act));
                g.drawEllipse (pos.x - ringR, pos.y - ringR, ringR * 2.0f, ringR * 2.0f, 1.5f);
            }
        }

        // 2. Dot outline at full colour (IEM: always visible regardless of hemisphere)
        juce::Path dotPath;
        dotPath.addEllipse (pos.x - half, pos.y - half, dotSize, dotSize);

        // Core brightness boost during activity pulse (prototype tapCorePulse: brightness 1→1.5→1.4→1)
        float coreBrightness = 1.0f;
        if (activityLevel[(size_t)i] > 0.01f)
        {
            float phase = pulsePhase[(size_t)i];
            if (phase < 0.03f)
                coreBrightness = 1.0f + (phase / 0.03f) * 0.5f;
            else if (phase < 0.08f)
                coreBrightness = 1.5f - ((phase - 0.03f) / 0.05f) * 0.1f;
            else if (phase < 0.18f)
                coreBrightness = 1.4f - ((phase - 0.08f) / 0.10f) * 0.4f;
        }
        auto coreColour = (coreBrightness > 1.01f)
                        ? objectColours[i].brighter (coreBrightness - 1.0f)
                        : objectColours[i];

        g.setColour (coreColour);
        g.strokePath (dotPath, juce::PathStrokeType (1.2f));

        // 3. Dot fill with hemisphere alpha (IEM: 1.0 above, 0.3 below)
        g.setColour (coreColour.withAlpha (isAbove ? 1.0f : 0.3f));
        g.fillPath (dotPath);

        // 4. Number label — Path-based faux bold with pixel-perfect centering
        //    Converts glyphs to a Path, then fills + strokes for guaranteed visual weight.
        //    GlyphArrangement getBoundingBox() centers on actual pixel bounds (no descent offset).
        {
            auto labelColour = isAbove ? juce::Colour (0xff161820) : objectColours[i];
            juce::Font labelFont = lf ? makeFont (lf->jetbrainsBold, 10.0f)
                                      : juce::Font (juce::FontOptions (10.0f));
            juce::GlyphArrangement glyphs;
            juce::String numText (i + 1);
            glyphs.addLineOfText (labelFont, numText, 0.0f, 0.0f);
            auto glyphBounds = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), true);
            float gx = pos.x - glyphBounds.getWidth() * 0.5f - glyphBounds.getX();
            float gy = pos.y - glyphBounds.getHeight() * 0.5f - glyphBounds.getY();
            glyphs.moveRangeOfGlyphs (0, -1, gx, gy);

            // Convert to Path and fill+stroke for faux bold effect
            juce::Path textPath;
            glyphs.createPath (textPath);
            g.setColour (labelColour);
            g.fillPath (textPath);
            g.strokePath (textPath, juce::PathStrokeType (0.8f));
        }

        // 5. Elevation degree label
        //    - Normal UI: selected object only, non-zero elevation
        //    - Screenshot mode (#168 round 2): every enabled object regardless of magnitude
        bool drawElevationLabel = labelAllEnabledForScreenshot
                                    ? objects[(size_t)i].enabled
                                    : (i == selectedObject && std::abs (elDeg) > 1.0f);
        if (drawElevationLabel)
        {
            float labelOffsetY = isAbove ? -(half + 14.0f) : (half + 2.0f);
            g.setColour (objectColours[i].withAlpha (0.85f));
            if (lf) g.setFont (makeFont (lf->jetbrainsRegular, 11.0f));
            else    g.setFont (juce::FontOptions (11.0f));
            juce::String elText = (elDeg > 0.0f ? "+" : "")
                                + juce::String (juce::roundToInt (elDeg))
                                + juce::String::charToString (0x00B0);
            g.drawText (elText, juce::roundToInt (pos.x - 18), juce::roundToInt (pos.y + labelOffsetY),
                        36, 12, juce::Justification::centred);
        }

        // 6. OSC override label (collision-aware positioning)
        if (oscOverride[(size_t)i])
        {
            float oscLabelY = (i == selectedObject && elDeg < -1.0f)
                            ? pos.y + half + 14.0f    // below elevation label
                            : pos.y + half + 1.0f;    // normal position
            g.setColour (accentStellar);
            if (lf) g.setFont (makeFont (lf->jetbrainsBold, 10.0f));
            else    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
            g.drawText ("OSC", juce::roundToInt (pos.x - half - 2), juce::roundToInt (oscLabelY),
                        juce::roundToInt (dotSize + 4), 10, juce::Justification::centred);
        }
    }
}

void SpatialMapComponent::mouseDown (const juce::MouseEvent& e)
{
    draggedObject = findObjectAt (e.position);
    if (draggedObject >= 0)
    {
        listeners.call ([this](Listener& l) { l.objectSelected (draggedObject); });
        // Issue E15: Signal drag start for gesture wrapping (undo grouping)
        if (onDragStarted) onDragStarted (draggedObject);
    }
}

void SpatialMapComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedObject < 0) return;

    auto result = pixelToSpatial (e.position);
    float azDeg = result.first;
    float dist  = result.second;
    objects[(size_t)draggedObject].azimuthDeg = azDeg;
    objects[(size_t)draggedObject].distance   = dist;
    repaint();

    listeners.call ([this, azDeg, dist](Listener& l) {
        l.objectPositionChanged (draggedObject, azDeg, dist);
    });
}

void SpatialMapComponent::mouseUp (const juce::MouseEvent&)
{
    // Issue E15: Signal drag end for gesture wrapping (undo grouping)
    if (draggedObject >= 0 && onDragEnded)
        onDragEnded (draggedObject);
    draggedObject = -1;
}

} // namespace spatialcore
