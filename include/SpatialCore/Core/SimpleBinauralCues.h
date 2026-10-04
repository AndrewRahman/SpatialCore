#pragma once

#include <cmath>

namespace spatialcore
{

//==============================================================================
// SimpleBinauralCues.h -- the position-dependent spectral cues of the Simple
// (Woodworth) binaural path (BUG-01, SpatialCore#15, D-01).
//
// Before this change the Simple path was a pair of broadband gains driven by the
// lateral angle (sin az cos el) alone, so a source behind, in front, overhead and
// underfoot all sounded the same. RenderEngine now runs the mono source through a
// blend of three fixed-coefficient filter branches before the pan gains:
//
//     y = x + wRear (R(x) - x) + wUp (U(x) - x) + wDown (D(x) - x)
//
// R is the rear head-shadow / pinna cut, U the overhead pinna cue, D the underfoot
// cue. The weights come from computeSimpleCueWeights(); the stage tables below are
// the filter designs. The filter STATE lives in RenderEngine (SpatializationAlgorithm
// is stateless and frozen, DR-7), so this header holds only pure functions and
// constants.
//
// Inputs must be finite. This header deliberately contains no finiteness checks:
// RenderEngine sanitises positions (hold-last-good) before any weight is computed,
// and the finiteness tests live in RenderEngine.cpp where the no-fast-math guard
// applies.
//
// FILTER SOURCE (Claude's Discretion record, 03-RESEARCH.md Pattern 1): the mean
// median-plane response differences of the five shipped SOFA files, corroborated by
// Blauert's directional bands and Hebrank & Wright 1974, with Brown & Duda 1998 as
// the structural precedent for a cascade of simple shelf / peak filters standing in
// for a pinna response.
//
// Design values are for 48 kHz and are recomputed by RenderEngine::prepare() for the
// actual sample rate, with every design frequency capped at 0.45 x the sample rate.
// Measured at full weight (research): Rear 4.49 dB RMS / 11.6 dB max, Up 2.70 / 8.4,
// Down 2.04 / 5.2 over the third-octave bands; DC gain 0.000 +- 0.001 dB for each
// branch.
//==============================================================================

/** Blend weights of the three cue branches, each in [0, 1]. */
struct SimpleCueWeights
{
    float rear = 0.0f;
    float up = 0.0f;
    float down = 0.0f;
};

/** Weights below this are snapped to exactly 0. In float, cos (degreesToRadians (90.0f))
    is -4.37e-8, so without the snap the rear weight at az +-90 would be 4.37e-8 and the
    ear-level front half (|az| <= 90) would no longer render bit-identical to the
    pre-change engine. */
inline constexpr float kSimpleCueWeightEpsilon = 1.0e-6f;

/** Position to branch weights. Azimuth 0 = front, positive = left; elevation 0 = ear
    level, positive = up (SpatialMath.cpp convention). Radians. Pure and stateless.

      wRear = max (0, -cos az cos el)   behind the listener
      wUp   = max (0,  sin el)          above
      wDown = max (0, -sin el)          below

    Every ear-level source in the front half (el 0, |az| <= 90) has all three weights
    exactly 0, so the cue bank is an exact pass-through there. */
inline SimpleCueWeights computeSimpleCueWeights (float azimuthRad, float elevationRad)
{
    const float cosEl = std::cos (elevationRad);
    const float sinEl = std::sin (elevationRad);

    SimpleCueWeights w;
    w.rear = std::fmax (0.0f, -std::cos (azimuthRad) * cosEl);
    w.up = std::fmax (0.0f, sinEl);
    w.down = std::fmax (0.0f, -sinEl);

    if (w.rear < kSimpleCueWeightEpsilon) w.rear = 0.0f;
    if (w.up < kSimpleCueWeightEpsilon) w.up = 0.0f;
    if (w.down < kSimpleCueWeightEpsilon) w.down = 0.0f;
    return w;
}

/** One biquad stage of a cue branch (RBJ high-shelf or peaking EQ). */
struct SimpleCueStage
{
    enum class Kind { HighShelf, Peak };

    Kind kind;
    float frequencyHz;   ///< design frequency at 48 kHz
    float q;
    float gainDb;
};

/** Rear branch: head shadow and pinna cut for sources behind the listener.
    Full-weight response -4.0 dB at 5 kHz. */
inline constexpr SimpleCueStage kSimpleCueRearStages[3] = {
    { SimpleCueStage::Kind::HighShelf, 2500.0f, 0.7f, -4.0f },
    { SimpleCueStage::Kind::HighShelf, 10000.0f, 0.7f, -8.0f },
    { SimpleCueStage::Kind::Peak, 1200.0f, 0.8f, 2.0f },
};

/** Up branch: the 7-9 kHz overhead pinna peak with a 3 kHz notch and a high shelf cut.
    Full-weight response +8.4 dB at 8 kHz. */
inline constexpr SimpleCueStage kSimpleCueUpStages[3] = {
    { SimpleCueStage::Kind::Peak, 8000.0f, 2.0f, 10.0f },
    { SimpleCueStage::Kind::Peak, 3000.0f, 0.8f, -4.0f },
    { SimpleCueStage::Kind::HighShelf, 11000.0f, 0.7f, -5.0f },
};

/** Down branch: a 5.5 kHz dip with a small 1.2 kHz lift for sources underfoot.
    Full-weight response -5.2 dB at 5.5 kHz. */
inline constexpr SimpleCueStage kSimpleCueDownStages[2] = {
    { SimpleCueStage::Kind::Peak, 5500.0f, 1.5f, -6.0f },
    { SimpleCueStage::Kind::Peak, 1200.0f, 0.7f, 2.5f },
};

} // namespace spatialcore
