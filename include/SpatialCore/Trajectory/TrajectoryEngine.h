#pragma once

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
    static TrajectoryResult compute(TrajectoryShape shape, float phase,
                                    float baseAzDeg, float baseElDeg, float baseDist,
                                    bool reverse = false);
    static int getNumShapes();
    static const char* getShapeName(TrajectoryShape shape);
};

} // namespace spatialcore
