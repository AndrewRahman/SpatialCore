#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <cstring>

namespace spatialcore
{

void PartitionedConvolver::prepare(int /*maxBlockSize*/, int /*irLength*/)
{
    // Stub — real FFT overlap-save setup during extraction
}

void PartitionedConvolver::setIR(const float* /*ir*/, int /*length*/)
{
    // Stub
}

void PartitionedConvolver::process(const float* /*in*/, float* out, int numSamples)
{
    std::memset(out, 0, sizeof(float) * static_cast<size_t>(numSamples));
}

void PartitionedConvolver::reset()
{
    inputAccumPos = 0;
    crossfadeRemaining = 0;
}

} // namespace spatialcore
