#include <SpatialCore/OSC/ADMOSCSender.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-02/D-20: queueReply and tick test isfinite; it must not fold away
#include <cmath>

namespace spatialcore
{

bool ADMOSCSender::connect(const juce::String& host, int port)
{
    sendHost = host;
    sendPort = port;
    connected = sender.connect(host, port);
    if (connected)
    {
        // Late joiners get a full picture: every enabled object is sent at the
        // next slot even if nothing moved (D-08b).
        forceSend_.fill(true);
        scheduleArmed_ = false;
    }
    return connected;
}

void ADMOSCSender::disconnect()
{
    sender.disconnect();
    connected = false;
    scheduleArmed_ = false;

    // A reply queued for the old connection must not leak into the next one.
    for (auto& perObject : pendingReply_)
        for (auto& r : perObject)
            r.pending = false;
}

void ADMOSCSender::queueReply(int objectIndex, ADMPositionQuery kind,
                              float azimuthDeg, float elevationDeg, float distance)
{
    const int k = static_cast<int>(kind);
    if (! connected || objectIndex < 0 || objectIndex >= MAX_SOURCES
        || k < 0 || k >= kNumQueryKinds)
        return;

    if (! std::isfinite(azimuthDeg) || ! std::isfinite(elevationDeg) || ! std::isfinite(distance))
        return;

    // Latest wins: the same (object, kind) overwrites, so N queries make one reply.
    auto& r = pendingReply_[objectIndex][k];
    r.pending = true;
    r.a = azimuthDeg;
    r.b = elevationDeg;
    r.c = distance;
}

void ADMOSCSender::flushReplies()
{
    static constexpr const char* kPropertyNames[] = { "azim", "elev", "dist", "aed", "xyz" };
    static_assert (sizeof (kPropertyNames) / sizeof (kPropertyNames[0]) == kNumQueryKinds,
                   "kPropertyNames must have one entry per ADMPositionQuery (IN-02)");
    static constexpr float kPi = 3.14159265358979323846f;

    // Object order, then kind order, so a burst of replies is deterministic.
    for (int obj = 0; obj < MAX_SOURCES; ++obj)
    {
        for (int k = 0; k < kNumQueryKinds; ++k)
        {
            auto& r = pendingReply_[obj][k];
            if (! r.pending)
                continue;
            r.pending = false;

            juce::OSCMessage msg("/adm/obj/" + juce::String(obj + 1) + "/" + kPropertyNames[k]);
            switch (static_cast<ADMPositionQuery>(k))
            {
                case ADMPositionQuery::azim: msg.addFloat32(r.a); break;
                case ADMPositionQuery::elev: msg.addFloat32(r.b); break;
                case ADMPositionQuery::dist: msg.addFloat32(r.c); break;
                case ADMPositionQuery::aed:
                    msg.addFloat32(r.a);
                    msg.addFloat32(r.b);
                    msg.addFloat32(r.c);
                    break;
                case ADMPositionQuery::xyz:
                {
                    // Inverse of the receiver's conversion (ITU-R BS.2127-0):
                    // x = -d cos(el) sin(az), y = d cos(el) cos(az), z = d sin(el).
                    const float az = r.a * (kPi / 180.0f);
                    const float el = r.b * (kPi / 180.0f);
                    msg.addFloat32(-r.c * std::cos(el) * std::sin(az));
                    msg.addFloat32( r.c * std::cos(el) * std::cos(az));
                    msg.addFloat32( r.c * std::sin(el));
                    break;
                }
            }
            sender.send(msg);
        }
    }
}

void ADMOSCSender::sendPosition(int objectIndex, float azimuthDeg,
                                 float elevationDeg, float distance)
{
    if (! connected || objectIndex < 0 || objectIndex >= MAX_SOURCES)
        return;

    // Build ADM-OSC address: /adm/obj/N/aed  (1-based)
    juce::String address = "/adm/obj/" + juce::String(objectIndex + 1) + "/aed";
    juce::OSCMessage msg(address);
    msg.addFloat32(azimuthDeg);
    msg.addFloat32(elevationDeg);
    msg.addFloat32(distance);
    sender.send(msg);
}

bool ADMOSCSender::consumeSendSlot(double nowSeconds)
{
    if (! scheduleArmed_)
    {
        // First call after connect(): send now and arm the schedule.
        scheduleArmed_ = true;
        nextSendDue_   = nowSeconds + kSendIntervalSeconds;
        return true;
    }

    // 1e-6 s tolerance so a caller at an exact multiple of 1/30 lands on the slot.
    if (nowSeconds + 1.0e-6 < nextSendDue_)
        return false;

    nextSendDue_ += kSendIntervalSeconds;

    // The caller stalled: resync instead of catching up (no burst).
    if (nowSeconds - nextSendDue_ >= kSendIntervalSeconds)
        nextSendDue_ = nowSeconds + kSendIntervalSeconds;

    return true;
}

void ADMOSCSender::tick(const float* azimuthsDeg, const float* elevationsDeg,
                         const float* distances, const bool* enabled, int numObjects)
{
    tick(azimuthsDeg, elevationsDeg, distances, enabled, numObjects,
         juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void ADMOSCSender::tick(const float* azimuthsDeg, const float* elevationsDeg,
                         const float* distances, const bool* enabled, int numObjects,
                         double nowSeconds)
{
    if (! connected)
        return;

    const int n = juce::jmin(numObjects, (int) MAX_SOURCES);

    // A disabled object is re-sent once when it is enabled again, even if the
    // disable lasted less than one slot.
    for (int i = 0; i < n; ++i)
        if (! enabled[i])
            forceSend_[(size_t) i] = true;

    if (! consumeSendSlot(nowSeconds))
        return;

    for (int i = 0; i < n; ++i)
    {
        if (! enabled[i])
            continue;

        // A non-finite position is never sent and never becomes the dead-band
        // reference: a NaN reference makes every later comparison false, which
        // would silence the object until it is re-enabled (WR-01). The previous
        // reference and forceSend_ are kept, so the next finite position sends.
        if (! std::isfinite(azimuthsDeg[i]) || ! std::isfinite(elevationsDeg[i])
            || ! std::isfinite(distances[i]))
            continue;

        // First position, connect and re-enable always send; afterwards only a
        // change beyond the dead-band does.
        const float dAz   = std::abs(azimuthsDeg[i]   - prevAz[i]);
        const float dEl   = std::abs(elevationsDeg[i] - prevEl[i]);
        const float dDist = std::abs(distances[i]     - prevDist[i]);

        if (forceSend_[(size_t) i] || dAz > 0.1f || dEl > 0.1f || dDist > 0.001f)
        {
            sendPosition(i, azimuthsDeg[i], elevationsDeg[i], distances[i]);

            prevAz[i]   = azimuthsDeg[i];
            prevEl[i]   = elevationsDeg[i];
            prevDist[i] = distances[i];
            forceSend_[(size_t) i] = false;
        }
    }

    // Query replies leave on the slot, after the position sends. They never touch
    // prevAz/prevEl/prevDist or forceSend_, so the dead-band is unaffected.
    flushReplies();
}

} // namespace spatialcore
