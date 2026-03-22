#pragma once

#include <juce_core/juce_core.h>

// Forward declaration — libmysofa is a private dependency
struct MYSOFA_EASY;

namespace spatialcore
{

class HRTFDatabase
{
public:
    HRTFDatabase();
    ~HRTFDatabase();

    bool loadFromMemory(const void* data, int dataSize, float targetSampleRate);

    void getInterpolatedHRIR(float azimuthRad, float elevationRad,
                             float* irL, float* irR,
                             float& delayL, float& delayR) const;

    void getAlignedHRIR(float azimuthRad, float elevationRad,
                        float* irL, float* irR,
                        float& delayL, float& delayR) const;

    int  getIRLength() const { return irLength; }
    int  getNumPositions() const { return numPositions; }
    bool isLoaded() const { return loaded; }
    void unload();

private:
    MYSOFA_EASY* easyHandle = nullptr;
    int irLength     = 0;
    int numPositions = 0;
    bool loaded      = false;
};

} // namespace spatialcore
