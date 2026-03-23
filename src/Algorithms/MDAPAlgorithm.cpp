#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <cstring>

namespace spatialcore
{
void MDAPAlgorithm::computeGains(const SourcePosition& /*source*/, const LayoutContext& /*ctx*/,
                                  float* outputGains, int numSpeakers) const
{
    std::memset(outputGains, 0, sizeof(float) * static_cast<size_t>(numSpeakers));
}
} // namespace spatialcore
