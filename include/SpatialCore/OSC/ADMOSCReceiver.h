#pragma once

#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <juce_osc/juce_osc.h>

namespace spatialcore
{

class ADMOSCReceiver : private juce::OSCReceiver,
                       private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void admPositionReceived(int objectIndex, float azimuthDeg,
                                         float elevationDeg, float distance) = 0;
    };

    ADMOSCReceiver();
    ~ADMOSCReceiver() override;

    bool connect(int port);
    void disconnect();
    bool isConnected() const { return connected; }
    int  getPort() const { return receivePort; }

    void addListener(Listener* l);
    void removeListener(Listener* l);

private:
    void oscMessageReceived(const juce::OSCMessage& message) override;

    int  receivePort = 4002;
    bool connected   = false;
    juce::ListenerList<Listener> listeners;
};

} // namespace spatialcore
