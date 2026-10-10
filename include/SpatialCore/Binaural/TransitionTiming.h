#pragma once

#include <algorithm>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// HRIR transition timing (issue #234, decisions D-01 / D-04 / D-06 / D-15).
//
// The binaural HRIR transition is defined in WALL-CLOCK time, never in host
// blocks, so a moving source transitions identically at every host buffer size
// and sample rate.  Both the PartitionedConvolver (equal-power HRIR crossfade,
// warmup floor) and the BinauralRenderer (ITD slew, plan 10-12) convert these
// constants with hrirTransitionMsToSamples(), so the two durations cannot drift
// apart.
//==============================================================================

/** Equal-power HRIR crossfade duration.  21.333 ms == exactly 1024 samples at
    48 kHz (the pre-#234 4 x 256-sample crossfade at a 256-sample host buffer). */
inline constexpr float kHRIRCrossfadeMs = 64.0f / 3.0f;

/** Warmup floor: the inactive convolver slot warms for max(IR length, this).
    5.333 ms == 256 samples at 48 kHz.  Deliberately NOT the host block size. */
inline constexpr float kHRIRWarmupMinMs = 16.0f / 3.0f;

/** Convert a millisecond duration to a sample count at the given rate.
    Rates outside (0, 1e6] fall back to 48 kHz.  The result is at least 1.
    Plain comparisons (not std::isfinite) so this stays correct in translation
    units compiled with -ffast-math. */
inline int hrirTransitionMsToSamples (double sampleRate, float ms) noexcept
{
    const double sr = (sampleRate > 0.0 && sampleRate <= 1.0e6) ? sampleRate : 48000.0;
    return std::max (1, static_cast<int> (std::lround (sr * static_cast<double> (ms) / 1000.0)));
}

} // namespace spatialcore
