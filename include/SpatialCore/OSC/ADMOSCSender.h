#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/OSC/ADMPositionQuery.h>
#include <juce_osc/juce_osc.h>
#include <array>

namespace spatialcore
{

class ADMOSCSender
{
public:
    /** The broadcast rate is fixed at 30 Hz whatever rate the caller's timer runs at. */
    static constexpr double kSendIntervalSeconds = 1.0 / 30.0;

    /** Dead-band of tick(): an enabled object is re-sent only when azimuth or elevation moved by more
        than kAngleDeadBandDeg, or distance by more than kDistanceDeadBand (a change of exactly the
        dead-band is not sent). The first position, connect and re-enable send regardless (IN-08). */
    static constexpr float kAngleDeadBandDeg  = 0.1f;
    static constexpr float kDistanceDeadBand  = 0.001f;

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

    /** Message thread only. Answers a position query (D-08a): the consumer calls this from
        ADMOSCReceiver::Listener::admPositionQueried with the object's current position.

        The reply mirrors the query (azim, elev and dist carry one float32, aed carries
        azimuth, elevation, distance, xyz carries the cartesian form of the same position),
        and is flushed on the next 30 Hz slot of tick(). One reply is pending per
        (object, kind) and the latest call wins, so a flood of queries produces one reply.
        Ignored while disconnected, for an out-of-range index, or for a non-finite value.
        A reply does not change the dead-band state of the position sends.

        The reply goes to this sender's configured host and port, NOT to the address the
        query came from. This is a stated deviation from the ADM-OSC text, because
        juce::OSCReceiver does not expose a packet's source address, and it means a
        spoofed query cannot reflect traffic at a third party (D-20). */
    void queueReply (int objectIndex, ADMPositionQuery kind,
                     float azimuthDeg, float elevationDeg, float distance);

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

    // One pending query reply per (object, kind); a fixed table, no allocation.
    struct PendingReply
    {
        bool  pending = false;
        float a = 0.0f, b = 0.0f, c = 0.0f;
    };
    // WR-01: counted from the enum's own sentinel, so a new kind added before kCount_ grows the
    // table by itself. kPropertyNames has a static_assert and the flushReplies switch has
    // -Wswitch to force an entry for it.
    static constexpr int kNumQueryKinds = static_cast<int> (ADMPositionQuery::kCount_);
    PendingReply pendingReply_[MAX_SOURCES][kNumQueryKinds] = {};

    void flushReplies();
    void clearPendingReplies();   // WR-11: replies belong to the destination they were queued for

    float prevAz[MAX_SOURCES]   = {};
    float prevEl[MAX_SOURCES]   = {};
    float prevDist[MAX_SOURCES] = {};
};

} // namespace spatialcore
