#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace spatialcore
{

class PartitionedConvolver
{
public:
    PartitionedConvolver() = default;

    void prepare(int maxBlockSize, int irLength);
    void setIR(const float* ir, int length);
    void process(const float* in, float* out, int numSamples);
    void reset();

    bool isPrepared() const { return fftSize > 0; }

private:
    juce::dsp::FFT fft { 1 };
    int fftOrder  = 1;
    int fftSize   = 0;
    int irLen     = 0;
    int blockSize = 0;

    std::vector<float> irFreqDomain;
    std::vector<float> inputAccum;
    std::vector<float> fftWorkBuf;
    std::vector<float> overlapBuf;
    int inputAccumPos = 0;

    // Dual-convolver crossfade for click-free IR updates
    std::vector<float> prevIrFreqDomain;
    std::vector<float> prevFftWorkBuf;
    std::vector<float> prevOverlapBuf;
    int crossfadeRemaining    = 0;
    int crossfadeTotalLength  = 0;
    static constexpr int kCrossfadeBlocks = 4;
};

} // namespace spatialcore
