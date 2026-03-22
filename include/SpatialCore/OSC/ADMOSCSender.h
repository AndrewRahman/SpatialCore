#pragma once

#include <SpatialCore/Core/Types.h>
#include <juce_osc/juce_osc.h>

namespace spatialcore
{

class ADMOSCSender
{
public:
    ADMOSCSender() = default;

    bool connect(const juce::String& host, int port);
    void disconnect();
    bool isConnected() const { return connected; }

    void sendPosition(int objectIndex, float azimuthDeg,
                      float elevationDeg, float distance);

    void tick(const float* azimuthsDeg, const float* elevationsDeg,
              const float* distances, const bool* enabled, int numObjects);

private:
    juce::OSCSender sender;
    bool connected = false;
    int  sendPort  = 4003;
    juce::String sendHost = "127.0.0.1";

    int   tickCounter = 0;
    float prevAz[MAX_SOURCES]   = {};
    float prevEl[MAX_SOURCES]   = {};
    float prevDist[MAX_SOURCES] = {};
};

} // namespace spatialcore
