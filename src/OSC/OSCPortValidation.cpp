#include <SpatialCore/OSC/OSCPortValidation.h>

namespace spatialcore
{

bool oscPortsConflict (int receivePort, int sendPort, const juce::String& sendHost)
{
    const bool isLoopback = sendHost == "127.0.0.1" || sendHost.equalsIgnoreCase ("localhost");
    return isLoopback && receivePort == sendPort;
}

} // namespace spatialcore
