#include <SpatialCore/OSC/ADMOSCSender.h>
#include <cmath>

namespace spatialcore
{

bool ADMOSCSender::connect(const juce::String& host, int port)
{
    sendHost = host;
    sendPort = port;
    connected = sender.connect(host, port);
    return connected;
}

void ADMOSCSender::disconnect()
{
    sender.disconnect();
    connected = false;
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

void ADMOSCSender::tick(const float* azimuthsDeg, const float* elevationsDeg,
                         const float* distances, const bool* enabled, int numObjects)
{
    if (! connected)
        return;

    // 30Hz gated broadcast: send every other tick (assuming 60Hz timer)
    ++tickCounter;
    if ((tickCounter & 1) != 0)
        return;

    for (int i = 0; i < numObjects && i < MAX_SOURCES; ++i)
    {
        if (! enabled[i])
            continue;

        // Only send if position has changed (dead-band threshold)
        float dAz   = std::abs(azimuthsDeg[i]   - prevAz[i]);
        float dEl   = std::abs(elevationsDeg[i]  - prevEl[i]);
        float dDist = std::abs(distances[i]      - prevDist[i]);

        if (dAz > 0.1f || dEl > 0.1f || dDist > 0.001f)
        {
            sendPosition(i, azimuthsDeg[i], elevationsDeg[i], distances[i]);

            prevAz[i]   = azimuthsDeg[i];
            prevEl[i]   = elevationsDeg[i];
            prevDist[i]  = distances[i];
        }
    }
}

} // namespace spatialcore
