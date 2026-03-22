#include <SpatialCore/OSC/ADMOSCReceiver.h>

namespace spatialcore
{

ADMOSCReceiver::ADMOSCReceiver()
{
    juce::OSCReceiver::addListener(this);
}

ADMOSCReceiver::~ADMOSCReceiver()
{
    disconnect();
}

bool ADMOSCReceiver::connect(int port)
{
    receivePort = port;
    connected = juce::OSCReceiver::connect(port);
    return connected;
}

void ADMOSCReceiver::disconnect()
{
    juce::OSCReceiver::disconnect();
    connected = false;
}

void ADMOSCReceiver::addListener(Listener* l)    { listeners.add(l); }
void ADMOSCReceiver::removeListener(Listener* l) { listeners.remove(l); }

void ADMOSCReceiver::oscMessageReceived(const juce::OSCMessage& /*message*/)
{
    // Stub — real ADM-OSC parsing during extraction
    // Parses /adm/obj/N/{azim,elev,dist,aed,xyz}
}

} // namespace spatialcore
