#pragma once

#include <SpatialCore/Core/Types.h>

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>

namespace spatialcore
{

/** Wrap azimuth into [-180, 180] range.

    Bounded for any input (WR-05): a value far from the range (1e10, 1e30) is
    reduced with one remainder first, because the subtract-360 loop alone never
    returns on it (1e10 - 360 == 1e10 in float). A NaN stays NaN and an infinity
    becomes NaN, so a non-finite value stays visibly non-finite for the caller's
    own guard instead of hanging the thread. Values within a few turns of the
    range take the same loop as before, so their results are unchanged. */
inline float wrapAzimuth (float az)
{
    if (std::abs (az) > 3600.0f)
        az = std::remainder (az, 360.0f);

    while (az > 180.0f)  az -= 360.0f;
    while (az < -180.0f) az += 360.0f;
    return az;
}

/** Unwrap an azimuth delta to shortest-path rotation (single wrap). */
inline float unwrapAzimuthDelta (float delta)
{
    if (delta > 180.0f)  delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return delta;
}

//==============================================================================
// Trajectory Animation Engine — Per-Object Spatial Movement
// Extracted from PluginProcessor for independent testability and reuse.
// Computes animated spatial positions from origin + trajectory shape.
//
// Thread contract (RTSF-02, SpatialCore#18):
//   - tick(), reset(), resetAll(), getState() and evaluateRandomNoise() run on ONE
//     thread (the timer or message thread). The state they touch other than the
//     final positions (phase_, noise_, rng_, ...) is not safe to read from another thread.
//   - getFinalAz/El/Dist() and isActive(), and the three final-position arrays
//     behind them, may be read from ANY thread, wait-free (lock-free atomics, no
//     mutex, spinlock or retry loop).
//   - Each value is published whole (never torn, and a Random azimuth is wrapped
//     before it is stored), but a reader may combine values from two consecutive
//     ticks: azimuth from one, elevation from the next. The size of that mixed
//     reading is bounded by one tick of motion, pinned per shape by the
//     [trajectory][step] test.
//   - The recommended consumer flow is still docs/integration-guide.md's: copy the
//     positions into per-object atomics on the message thread, and let the audio
//     thread read those.
//==============================================================================
class TrajectoryEngine
{
public:
    // Unified with MAX_SOURCES by SC-1 (AndrewRahman/SpatialCore#3) so a trajectory slot exists for every
    // object slot; never give it its own value.
    static constexpr int kMaxObjects = MAX_SOURCES;

    struct TrajectoryResult
    {
        float azDeg, elDeg, dist;
        bool controlsAz, controlsEl, controlsDist;
    };

    struct RandomNoiseState
    {
        float freqAz[4]  = {}, phaseAz[4]  = {}, ampAz[4]  = {};
        float freqEl[4]  = {}, phaseEl[4]  = {}, ampEl[4]  = {};
        float freqDist[3]= {}, phaseDist[3]= {}, ampDist[3]= {};
        bool initialized = false;
    };

    struct TrajectoryState
    {
        float originAzDeg  = 0.0f;
        float originElDeg  = 0.0f;
        float originDist   = 0.5f;
        int   shape        = 0;
        float phase        = 0.0f;
        bool  reverse      = false;
        float randomTime   = 0.0f;
    };

    struct RandomPosition { float azDeg, elDeg, dist; };

    /** Input parameters for a single object's tick. */
    struct ObjectInput
    {
        int   shape = 0;
        float speed = 1.0f;
        bool  reverse = false;
        float originAz = 0.0f;
        float originEl = 0.0f;
        float originDist = 0.5f;
        bool  oscOverride = false;
    };

    /** Advance animation for a single object (call from timer thread at ~60Hz). */
    void tick (int objectIndex, const ObjectInput& input, float dt = 1.0f / 60.0f);

    /** Pure function: compute trajectory position for any shape/phase. */
    static TrajectoryResult computeTrajectory (int shape, float phase,
                                                float baseAz, float baseEl, float baseDist,
                                                bool reverse = false);

    /** Get the current animated position for an object. */
    float getFinalAz   (int i) const { return finalAz_[i].load (std::memory_order_relaxed); }
    float getFinalEl   (int i) const { return finalEl_[i].load (std::memory_order_relaxed); }
    float getFinalDist (int i) const { return finalDist_[i].load (std::memory_order_relaxed); }
    bool  isActive     (int i) const { return active_[i].load (std::memory_order_acquire); }

    /** Get visualization state (used by editor). */
    TrajectoryState getState (int objectIndex) const;

    /** Evaluate random noise at arbitrary time (used by editor path preview). */
    RandomPosition evaluateRandomNoise (int objectIndex, float time) const;

    /** Reset a single object (e.g., after preset change). */
    void reset (int objectIndex, float azDeg, float elDeg, float dist);

    /** Reset all objects. */
    void resetAll();

    // Final position arrays: written by the one tick/reset thread, readable from any
    // thread (see the thread contract above). Each value is stored once, finished.
    // active_ is stored with release after the three values and loaded with acquire.
    static_assert (std::atomic<float>::is_always_lock_free,
                   "the audio-side read of trajectory positions must be wait-free");
    std::atomic<float> finalAz_[kMaxObjects]   = {};
    std::atomic<float> finalEl_[kMaxObjects]   = {};
    std::atomic<float> finalDist_[kMaxObjects] = {};
    std::atomic<bool> active_[kMaxObjects] = {};

    // State for getTrajectoryState() visualization
    float baseAzimuth_[kMaxObjects]   = {};
    float baseElevation_[kMaxObjects] = {};
    float baseDistance_[kMaxObjects]   = {};
    float phase_[kMaxObjects]         = {};
    int   prevShape_[kMaxObjects]     = {};
    float randomTime_[kMaxObjects]    = {};

    RandomNoiseState noise_[kMaxObjects] = {};
    juce::Random rng_;
};

} // namespace spatialcore
