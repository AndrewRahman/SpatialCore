#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>

namespace spatialcore
{

enum class TrajectoryShape
{
    None = 0,
    Bounce, Circle, Cross, Figure8, Heart, Helix,
    Infinity, Line, Orbit, Random, Spiral, Square, Triangle,
    NumShapes
};

struct TrajectoryResult
{
    float azDeg  = 0.0f;
    float elDeg  = 0.0f;
    float dist   = 1.0f;
    bool controlsAz   = false;
    bool controlsEl   = false;
    bool controlsDist  = false;
};

class TrajectoryEngine
{
public:
    static constexpr int kMaxObjects = 12;

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

    /** Pure function: compute trajectory position for any shape/phase. */
    static TrajectoryResult compute(TrajectoryShape shape, float phase,
                                    float baseAzDeg, float baseElDeg, float baseDist,
                                    bool reverse = false);

    static int getNumShapes();
    static const char* getShapeName(TrajectoryShape shape);

    /** Advance animation for a single object (call from timer thread at ~60Hz). */
    void tick(int objectIndex, const ObjectInput& input, float dt = 1.0f / 60.0f);

    /** Get the current animated position for an object. */
    float getFinalAz  (int i) const { return finalAz_[i]; }
    float getFinalEl  (int i) const { return finalEl_[i]; }
    float getFinalDist(int i) const { return finalDist_[i]; }
    bool  isActive    (int i) const { return active_[i].load(std::memory_order_relaxed); }

    /** Get visualization state (used by editor). */
    TrajectoryState getState(int objectIndex) const;

    /** Evaluate random noise at arbitrary time (used by editor path preview). */
    RandomPosition evaluateRandomNoise(int objectIndex, float time) const;

    /** Reset a single object (e.g., after preset change). */
    void reset(int objectIndex, float azDeg, float elDeg, float dist);

    /** Reset all objects. */
    void resetAll();

    // Direct access to final position arrays (read by processBlock)
    float finalAz_[kMaxObjects]   = {};
    float finalEl_[kMaxObjects]   = {};
    float finalDist_[kMaxObjects] = {};
    std::atomic<bool> active_[kMaxObjects] = {};

    // State for getState() visualization
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
