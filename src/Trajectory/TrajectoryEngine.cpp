#include <SpatialCore/Trajectory/TrajectoryEngine.h>

namespace spatialcore
{

static const char* kShapeNames[] = {
    "None", "Bounce", "Circle", "Cross", "Figure-8", "Heart", "Helix",
    "Infinity", "Line", "Orbit", "Random", "Spiral", "Square", "Triangle"
};

TrajectoryResult TrajectoryEngine::compute(TrajectoryShape /*shape*/, float /*phase*/,
                                            float baseAzDeg, float baseElDeg, float baseDist,
                                            bool /*reverse*/)
{
    // Stub — returns base position unchanged
    return { baseAzDeg, baseElDeg, baseDist, false, false, false };
}

int TrajectoryEngine::getNumShapes()
{
    return static_cast<int>(TrajectoryShape::NumShapes);
}

const char* TrajectoryEngine::getShapeName(TrajectoryShape shape)
{
    int idx = static_cast<int>(shape);
    if (idx < 0 || idx >= static_cast<int>(TrajectoryShape::NumShapes))
        return "Unknown";
    return kShapeNames[idx];
}

} // namespace spatialcore
