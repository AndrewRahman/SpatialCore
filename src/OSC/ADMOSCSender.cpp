#include <SpatialCore/OSC/ADMOSCSender.h>

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

void ADMOSCSender::sendPosition(int /*objectIndex*/, float /*azimuthDeg*/,
                                 float /*elevationDeg*/, float /*distance*/)
{
    // Stub — real /adm/obj/N/aed send during extraction
}

void ADMOSCSender::tick(const float* /*azimuthsDeg*/, const float* /*elevationsDeg*/,
                         const float* /*distances*/, const bool* /*enabled*/, int /*numObjects*/)
{
    // Stub — real 30Hz gated broadcast during extraction
}

} // namespace spatialcore
