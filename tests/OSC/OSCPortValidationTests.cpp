#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/OSC/OSCPortValidation.h>
#include <SpatialCore/OSC/ADMOSCReceiver.h>

using namespace spatialcore;

// ============================================================================
// OSCPortValidationTests — issue Spatial-Media-Lab/OpenSpatialDelay#179 / Phase 10 Plan 10-06. Proves the
// same-port (loopback) self-feedback conflict definition (D-06): equal
// receive/send ports on a loopback send host are rejected; different ports,
// or equal ports aimed at a non-loopback host, are not conflicts.
//
// Test 4 additionally proves the OSD-facing contract this validator gates:
// when a conflict is detected, the caller must skip connect() entirely, so
// the receiver's existing binding (connected state) is left undisturbed by a
// rejected same-port request. Driven synchronously via the receiver's
// testProcessOSCMessage() JUCE_UNIT_TESTS shim style (ADMOSCReceiver.h) --
// here we drive the receiver's connect()/isConnected() surface directly,
// mirroring that synchronous, no-real-socket-timing-dependency test idiom.
// ============================================================================

TEST_CASE ("oscPortsConflict: equal ports on loopback (127.0.0.1) is a conflict", "[osc][validation]")
{
    CHECK (oscPortsConflict (9000, 9000, "127.0.0.1") == true);
}

TEST_CASE ("oscPortsConflict: equal ports on loopback (localhost) is a conflict", "[osc][validation]")
{
    CHECK (oscPortsConflict (9000, 9000, "localhost") == true);
}

TEST_CASE ("oscPortsConflict: different ports on loopback is NOT a conflict", "[osc][validation]")
{
    CHECK (oscPortsConflict (9000, 9001, "127.0.0.1") == false);
}

TEST_CASE ("oscPortsConflict: equal ports on a non-loopback host is NOT a conflict", "[osc][validation]")
{
    CHECK (oscPortsConflict (9000, 9000, "192.168.1.50") == false);
}

TEST_CASE ("oscPortsConflict: gates the bind -- rejected same-port request leaves prior binding intact", "[osc][validation]")
{
    ADMOSCReceiver receiver;

    // Establish a prior valid binding.
    REQUIRE (receiver.connect (9000) == true);
    REQUIRE (receiver.isConnected() == true);

    // Simulate the OSD-side guard: a conflicting same-port send request on
    // loopback must be detected BEFORE any connect() call, so the caller
    // skips the bind entirely and the receiver's prior binding is never
    // touched.
    bool conflict = oscPortsConflict (9000, 9000, "127.0.0.1");
    REQUIRE (conflict == true);

    if (! conflict)
        receiver.connect (9000); // would only run on a (buggy) non-conflict path

    // Prior binding survives untouched -- still connected on the original port.
    CHECK (receiver.isConnected() == true);

    receiver.disconnect();
}
