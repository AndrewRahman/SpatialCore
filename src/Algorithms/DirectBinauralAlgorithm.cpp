#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <cstring>

namespace spatialcore
{
void DirectBinauralAlgorithm::computeGains(const SourcePosition& /*source*/, const LayoutContext& /*ctx*/,
                                            float* outputGains, int numSpeakers) const
{
    std::memset(outputGains, 0, sizeof(float) * static_cast<size_t>(numSpeakers));
}

BinauralGains DirectBinauralAlgorithm::computeBinauralGains(const SourcePosition& /*source*/,
                                                             const BinauralContext& /*ctx*/) const
{
    return {};
}
} // namespace spatialcore
