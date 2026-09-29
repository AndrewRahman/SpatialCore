#pragma once

#include <juce_core/juce_core.h>

namespace spatialcore
{

// oscPortsConflict — same-port OSC self-feedback validator (Phase 10 Plan
// 10-06, issue Spatial-Media-Lab/OpenSpatialDelay#179). Per D-06 the primary conflict is the OSC receive port
// colliding with the OSC send port WHEN the send host is loopback: binding
// both endpoints to the same UDP port on 127.0.0.1 makes the plugin
// re-receive its own broadcasts (a message storm / dead OSC). Equal ports
// aimed at a genuinely remote host are not a self-feedback hazard, so the
// host is taken into account rather than a blind receivePort == sendPort
// check (RESEARCH D-06 nuance).
//
// This is a standalone free function, NOT a change to ADMOSCReceiver::connect
// or ADMOSCSender::connect — both keep their existing bool-returning,
// connected-tracking contract (ADMOSCReceiver.h:57-61, ADMOSCSender.cpp:7-13).
// OSD calls this validator BEFORE invoking either connect() and hard-rejects
// the conflicting bind on true (D-07): skip the bind, keep the prior valid
// binding intact, surface an inline error in the OSC section UI.
//
// Returns true when sendHost is loopback (127.0.0.1 or localhost) AND
// receivePort == sendPort.
bool oscPortsConflict (int receivePort, int sendPort, const juce::String& sendHost);

} // namespace spatialcore
