#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <vector>

namespace spatialcore
{

class BinauralRenderer
{
public:
    BinauralRenderer() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void setProfile(int profileIndex, HRTFDatabase& hrtfDb);
    void updateSourceHRIR(int sourceIndex, float azRad, float elRad,
                          HRTFDatabase& db);
    void renderSourceBuffers(const float* const* sourceBufs,
                             const bool* sourceEnabled,
                             int numSources, int numSamples,
                             float* outL, float* outR);
    bool isSimpleMode() const { return activeProfile == 0; }
    int  getActiveProfile() const { return activeProfile; }
    void reset();

private:
    PartitionedConvolver sourceConvL[MAX_SOURCES];
    PartitionedConvolver sourceConvR[MAX_SOURCES];
    float cachedSourceAz[MAX_SOURCES] = {};
    float cachedSourceEl[MAX_SOURCES] = {};
    bool  sourceConvReady[MAX_SOURCES] = {};
    float storedNormGain  = 1.0f;
    int   storedIRLength  = 0;

    int    activeProfile    = 0;
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    std::vector<float> convTmpL, convTmpR;

    // ITD tracking for smooth HRIR transitions
    float currentITDL[MAX_SOURCES] = {};
    float currentITDR[MAX_SOURCES] = {};
    float targetITDL[MAX_SOURCES]  = {};
    float targetITDR[MAX_SOURCES]  = {};

    static constexpr int kITDBufferSize = 64;
    float itdBufferL[MAX_SOURCES][kITDBufferSize] = {};
    float itdBufferR[MAX_SOURCES][kITDBufferSize] = {};
    int   itdWritePos[MAX_SOURCES] = {};
    bool  itdActive = false;
};

} // namespace spatialcore
