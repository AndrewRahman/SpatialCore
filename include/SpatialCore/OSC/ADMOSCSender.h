#pragma once

#include <SpatialCore/Core/Types.h>
#include <juce_osc/juce_osc.h>
#include <array>

namespace spatialcore
{

class ADMOSCSender
{
public:
    /** The broadcast rate is fixed at 30 Hz whatever rate the caller's timer runs at. */
    static constexpr double kSendIntervalSeconds = 1.0 / 30.0;

    ADMOSCSender() { forceSend_.fill (true); }

    bool connect(const juce::String& host, int port);
    void disconnect();
    bool isConnected() const { return connected; }

    void sendPosition(int objectIndex, float azimuthDeg,
                      float elevationDeg, float distance);

    /** Timer entry point. Reads the wall clock and forwards to the overload below. */
    void tick(const float* azimuthsDeg, const float* elevationsDeg,
              const float* distances, const bool* enabled, int numObjects);

    /** Message thread only. Call from a timer at any rate of 30 Hz or more.

        The sender keeps its own clock (nowSeconds, any monotonic origin): it sends
        at most one message per object per 1/30 s slot and never catches up after
        the caller stalls. Objects whose position has not changed are not re-sent.
        An object's first position, every enabled object after connect(), and an
        object that is enabled again after being disabled are each sent once
        regardless of the dead-band (D-07, D-08b, D-08c, D-09). */
    void tick(const float* azimuthsDeg, const float* elevationsDeg,
              const float* distances, const bool* enabled, int numObjects,
              double nowSeconds);

private:
    // True when this call is a send slot; arms and advances the schedule.
    bool consumeSendSlot (double nowSeconds);

    juce::OSCSender sender;
    bool connected = false;
    int  sendPort  = 4003;
    juce::String sendHost = "127.0.0.1";

    double nextSendDue_    = 0.0;
    bool   scheduleArmed_  = false;
    std::array<bool, MAX_SOURCES> forceSend_ {};

    float prevAz[MAX_SOURCES]   = {};
    float prevEl[MAX_SOURCES]   = {};
    float prevDist[MAX_SOURCES] = {};
};

} // namespace spatialcore
