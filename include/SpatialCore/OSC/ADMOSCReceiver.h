#pragma once

#include <SpatialCore/Core/Types.h>
#include <juce_osc/juce_osc.h>

namespace spatialcore
{

// Which position property a query asked for. ADM-OSC rule: a message with no
// arguments sent to a position address is a query for the current value, and the
// receiver of the query answers with the same address carrying that value
// (Phase 4, D-08a). Reported through Listener::admPositionQueried.
enum class ADMPositionQuery { azim, elev, dist, aed, xyz };

// ADMOSCReceiver -- ADM-OSC + OSD-custom message parsing, decoupled from
// juce::AudioProcessor (Phase 8 Plan 08-05, CORE-05). Structural change only:
// this class OWNS a juce::OSCReceiver and implements
// juce::OSCReceiver::Listener on ITSELF (rather than the previous mixin on
// the plugin processor). Message-parsing logic (address grammar, clamping,
// Cartesian<->polar conversion) is moved verbatim from
// OpenSpatialDelayProcessor::oscMessageReceived (Source/PluginProcessor.cpp)
// -- see Pitfall 4 in 08-RESEARCH.md: this is an inheritance-shape change
// only, not a behavior change.
//
// The plugin-specific string-to-parameter mapping (e.g. "/osd/global/drywet"
// -> APVTS param ID "dryWet") stays owned by the consumer via the Listener
// callback below -- ADMOSCReceiver only decodes the wire-format grammar into
// generic (object index, property, value) / position events, keeping this
// class reusable across Spatial Media Library plugins that may expose a
// different parameter set.
class ADMOSCReceiver : private juce::OSCReceiver,
                       private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    // Callback interface the consumer (e.g. OpenSpatialDelayProcessor) supplies
    // to receive decoded ADM-OSC / OSD-custom events, keeping this class
    // independent of juce::AudioProcessor.
    struct Listener
    {
        virtual ~Listener() = default;

        // Per-object position update (/adm/obj/N/azim|elev|dist|aed|xyz|x|y|z
        // or /osd/obj/N/ aliases). objectIndex is 0-based.
        //
        // Single-axis messages (/azim, /elev, /dist) forward the two axes that were
        // NOT sent as NaN. The consumer MUST keep its stored value for any NaN axis
        // (test with std::isnan). The receiver never forwards a NaN that came from
        // the wire (D-21: a non-finite argument drops the whole message), so a NaN
        // here always means "axis not sent". Axes that are present are within
        // azimuthDeg [-180, 180], elevationDeg [-90, 90], distance [0, 1].
        virtual void admPositionReceived (int objectIndex, float azimuthDeg,
                                          float elevationDeg, float distance) = 0;

        // Per-object non-position param (/osd/obj/N/enabled|doppler|pitch|
        // trajectory|speed|direction|input). objectIndex is 0-based; paramName
        // is the raw OSC property name (e.g. "enabled", "doppler") -- the
        // consumer maps this to its own parameter ID scheme.
        //
        // value is finite (a NaN or infinity drops the message, D-21) and is a
        // float32 or an int32 converted to float, but it is otherwise UNBOUNDED:
        // /doppler, /pitch and /speed are not clamped here, and the range of
        // enabled/trajectory/direction/input depends on the consumer. Datagrams are
        // unauthenticated, so range-limit the value before using it (and never feed
        // it to a loop whose length depends on its size).
        virtual void admObjectParamReceived (int /*objectIndex*/, const juce::String& /*paramName*/,
                                             float /*value*/) {}

        // Global param (/osd/global/<property>). propertyName is the raw OSC
        // property name (e.g. "delaytime", "drywet") -- the consumer maps
        // this to its own parameter ID scheme. value is finite but otherwise
        // unbounded; range-limit it before use (see admObjectParamReceived).
        virtual void admGlobalParamReceived (const juce::String& /*propertyName*/, float /*value*/) {}

        // A device sent /adm/obj/N/ or /osd/obj/N/ azim|elev|dist|aed|xyz with no
        // arguments: it asks for the object's current position. Message thread.
        // objectIndex is 0-based. The consumer answers from its own state with
        // ADMOSCSender::queueReply; this class stores no position and holds no
        // processor pointer (DR-16, D-08). Defaulted, so existing consumers compile
        // and simply never answer.
        virtual void admPositionQueried (int /*objectIndex*/, ADMPositionQuery /*kind*/) {}
    };

    ADMOSCReceiver() { juce::OSCReceiver::addListener (this); }
    ~ADMOSCReceiver() override { disconnect(); }

    bool connect (int port)
    {
        connected = juce::OSCReceiver::connect (port);
        return connected;
    }
    void disconnect()
    {
        juce::OSCReceiver::disconnect();
        connected = false;
    }
    bool isConnected() const { return connected; }

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

#if JUCE_UNIT_TESTS
    // v1.0-carried test entry point — forwards to oscMessageReceived, mirrors
    // the pre-move OSD processor's public testProcessOSCMessage() shim so
    // SpatialCore's own OSC tests can drive parsing synchronously without a
    // real UDP socket.
    void testProcessOSCMessage (const juce::OSCMessage& msg) { oscMessageReceived (msg); }
#endif

private:
    void oscMessageReceived (const juce::OSCMessage& message) override;

    bool connected = false;
    juce::ListenerList<Listener> listeners;
};

} // namespace spatialcore
