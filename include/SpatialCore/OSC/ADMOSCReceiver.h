#pragma once

#include <SpatialCore/Core/Types.h>
#include <juce_osc/juce_osc.h>

namespace spatialcore
{

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
        virtual void admPositionReceived (int objectIndex, float azimuthDeg,
                                          float elevationDeg, float distance) = 0;

        // Per-object non-position param (/osd/obj/N/enabled|doppler|pitch|
        // trajectory|speed|direction|input). objectIndex is 0-based; paramName
        // is the raw OSC property name (e.g. "enabled", "doppler") -- the
        // consumer maps this to its own parameter ID scheme.
        virtual void admObjectParamReceived (int objectIndex, const juce::String& paramName,
                                             float value) {}

        // Global param (/osd/global/<property>). propertyName is the raw OSC
        // property name (e.g. "delaytime", "drywet") -- the consumer maps
        // this to its own parameter ID scheme.
        virtual void admGlobalParamReceived (const juce::String& propertyName, float value) {}
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
