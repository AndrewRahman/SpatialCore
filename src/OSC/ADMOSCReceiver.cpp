#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <cmath>

namespace spatialcore
{

//==============================================================================
// Cartesian->Polar conversion per ITU-R BS.2127-0
//==============================================================================
static void cartesianToPolar(float x, float y, float z,
                             float& azDeg, float& elDeg, float& dist)
{
    static constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    azDeg = std::atan2(-x, y) * kRadToDeg;
    float r = std::sqrt(x * x + y * y);
    elDeg = std::atan2(z, r) * kRadToDeg;
    dist  = std::sqrt(x * x + y * y + z * z);
    if (dist > 1.0f) dist = 1.0f;
    if (dist < 0.0f) dist = 0.0f;
}

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

void ADMOSCReceiver::oscMessageReceived(const juce::OSCMessage& message)
{
    auto address = message.getAddressPattern().toString();

    // Parse /adm/obj/N/... or /osd/obj/N/... addresses
    if (! address.startsWith("/adm/obj/") && ! address.startsWith("/osd/obj/"))
        return;

    auto afterObj = address.substring(9);  // both prefixes are 9 chars
    auto slashIdx = afterObj.indexOf("/");
    if (slashIdx < 0) return;

    int objNum = afterObj.substring(0, slashIdx).getIntValue();
    if (objNum < 1) return;

    int objectIndex = objNum - 1;
    if (objectIndex >= MAX_SOURCES) return;

    auto property = afterObj.substring(slashIdx);

    float azDeg = 0.0f, elDeg = 0.0f, dist = 1.0f;
    bool valid = false;

    if (property == "/azim" && message.size() >= 1 && message[0].isFloat32())
    {
        azDeg = message[0].getFloat32();
        valid = true;
    }
    else if (property == "/elev" && message.size() >= 1 && message[0].isFloat32())
    {
        elDeg = message[0].getFloat32();
        valid = true;
    }
    else if (property == "/dist" && message.size() >= 1 && message[0].isFloat32())
    {
        dist = message[0].getFloat32();
        valid = true;
    }
    else if (property == "/aed" && message.size() >= 3
             && message[0].isFloat32() && message[1].isFloat32() && message[2].isFloat32())
    {
        azDeg = message[0].getFloat32();
        elDeg = message[1].getFloat32();
        dist  = message[2].getFloat32();
        valid = true;
    }
    else if (property == "/xyz" && message.size() >= 3
             && message[0].isFloat32() && message[1].isFloat32() && message[2].isFloat32())
    {
        cartesianToPolar(message[0].getFloat32(), message[1].getFloat32(),
                         message[2].getFloat32(), azDeg, elDeg, dist);
        valid = true;
    }

    if (valid)
    {
        listeners.call([objectIndex, azDeg, elDeg, dist](Listener& l)
        {
            l.admPositionReceived(objectIndex, azDeg, elDeg, dist);
        });
    }
}

} // namespace spatialcore
