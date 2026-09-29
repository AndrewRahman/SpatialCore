#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <cmath>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Listener -- records the last decoded event of each kind, mirroring the
// consumer-side callback a real plugin (e.g. OpenSpatialDelayProcessor) would
// implement. Adapted from Tests/OscTests.cpp's createOscProcessor() pattern
// (Phase 8 Plan 08-05) but constructs ADMOSCReceiver directly instead of a
// full juce::AudioProcessor.
// ============================================================================
struct RecordingListener : public ADMOSCReceiver::Listener
{
    int lastPositionObjectIndex = -1;
    float lastAz = NAN, lastEl = NAN, lastDist = NAN;
    int positionCallCount = 0;

    int lastParamObjectIndex = -1;
    juce::String lastParamName;
    float lastParamValue = 0.0f;
    int paramCallCount = 0;

    juce::String lastGlobalProperty;
    float lastGlobalValue = 0.0f;
    int globalCallCount = 0;

    void admPositionReceived (int objectIndex, float azimuthDeg, float elevationDeg, float distance) override
    {
        lastPositionObjectIndex = objectIndex;
        lastAz = azimuthDeg;
        lastEl = elevationDeg;
        lastDist = distance;
        ++positionCallCount;
    }

    void admObjectParamReceived (int objectIndex, const juce::String& paramName, float value) override
    {
        lastParamObjectIndex = objectIndex;
        lastParamName = paramName;
        lastParamValue = value;
        ++paramCallCount;
    }

    void admGlobalParamReceived (const juce::String& propertyName, float value) override
    {
        lastGlobalProperty = propertyName;
        lastGlobalValue = value;
        ++globalCallCount;
    }
};

static std::unique_ptr<ADMOSCReceiver> createTestReceiver (RecordingListener& listener)
{
    auto receiver = std::make_unique<ADMOSCReceiver>();
    receiver->addListener (&listener);
    return receiver;
}

static void sendOSC (ADMOSCReceiver& receiver, const juce::String& address,
                     std::initializer_list<float> values)
{
    juce::OSCMessage msg (address);
    for (float v : values)
        msg.addFloat32 (v);
    receiver.testProcessOSCMessage (msg);
}

// ============================================================================
// Section 1: ADM-OSC Position Receive (/adm/obj/N/) -- single-axis messages
// forward NaN sentinels on the un-received axes (the consumer fills them from
// its own cached state, mirroring the pre-move OSD handler's exact behavior).
// ============================================================================

TEST_CASE ("ADMOSCReceiver: /adm/obj/N/azim sets azimuth, elev/dist are NaN", "[osc][position]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/adm/obj/1/azim", { 90.0f });

    REQUIRE (listener.positionCallCount == 1);
    CHECK (listener.lastPositionObjectIndex == 0);  // 1-based -> 0-based
    CHECK_THAT (listener.lastAz, WithinAbs (90.0f, 0.5f));
    CHECK (std::isnan (listener.lastEl));
    CHECK (std::isnan (listener.lastDist));
}

TEST_CASE ("ADMOSCReceiver: /adm/obj/N/elev sets elevation, az/dist are NaN", "[osc][position]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/adm/obj/2/elev", { 45.0f });

    REQUIRE (listener.positionCallCount == 1);
    CHECK (listener.lastPositionObjectIndex == 1);
    CHECK_THAT (listener.lastEl, WithinAbs (45.0f, 0.5f));
    CHECK (std::isnan (listener.lastAz));
    CHECK (std::isnan (listener.lastDist));
}

TEST_CASE ("ADMOSCReceiver: /adm/obj/N/dist sets distance, az/el are NaN", "[osc][position]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/adm/obj/3/dist", { 0.75f });

    REQUIRE (listener.positionCallCount == 1);
    CHECK (listener.lastPositionObjectIndex == 2);
    CHECK_THAT (listener.lastDist, WithinAbs (0.75f, 0.01f));
    CHECK (std::isnan (listener.lastAz));
    CHECK (std::isnan (listener.lastEl));
}

TEST_CASE ("ADMOSCReceiver: /adm/obj/N/aed sets all three position axes", "[osc][position]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/adm/obj/4/aed", { -45.0f, 30.0f, 0.6f });

    REQUIRE (listener.positionCallCount == 1);
    CHECK (listener.lastPositionObjectIndex == 3);
    CHECK_THAT (listener.lastAz, WithinAbs (-45.0f, 0.5f));
    CHECK_THAT (listener.lastEl, WithinAbs (30.0f, 0.5f));
    CHECK_THAT (listener.lastDist, WithinAbs (0.6f, 0.01f));
}

TEST_CASE ("ADMOSCReceiver: /adm/obj/N/xyz converts Cartesian to polar", "[osc][position]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    // Straight ahead: x=0, y=1, z=0 -> az=0, el=0, dist=1
    sendOSC (*receiver, "/adm/obj/1/xyz", { 0.0f, 1.0f, 0.0f });

    REQUIRE (listener.positionCallCount == 1);
    CHECK_THAT (listener.lastAz, WithinAbs (0.0f, 0.5f));
    CHECK_THAT (listener.lastEl, WithinAbs (0.0f, 0.5f));
    CHECK_THAT (listener.lastDist, WithinAbs (1.0f, 0.01f));
}

// ============================================================================
// Section 2: /osd/obj/N/ Position Aliases (receive-only)
// ============================================================================

TEST_CASE ("ADMOSCReceiver: /osd/obj/N/azim alias behaves identically to /adm/", "[osc][alias]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/1/azim", { -120.0f });

    REQUIRE (listener.positionCallCount == 1);
    CHECK_THAT (listener.lastAz, WithinAbs (-120.0f, 0.5f));
}

// ============================================================================
// Section 3: /osd/obj/N/ Per-Object Non-Position Params
// ============================================================================

TEST_CASE ("ADMOSCReceiver: /osd/obj/N/enabled forwards raw paramName+value", "[osc][per-object]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/1/enabled", { 1.0f });

    REQUIRE (listener.paramCallCount == 1);
    CHECK (listener.lastParamObjectIndex == 0);
    CHECK (listener.lastParamName == "enabled");
    CHECK_THAT (listener.lastParamValue, WithinAbs (1.0f, 0.01f));
}

TEST_CASE ("ADMOSCReceiver: /osd/obj/N/doppler forwards paramName 'doppler'", "[osc][per-object]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/2/doppler", { 0.75f });

    REQUIRE (listener.paramCallCount == 1);
    CHECK (listener.lastParamObjectIndex == 1);
    CHECK (listener.lastParamName == "doppler");
    CHECK_THAT (listener.lastParamValue, WithinAbs (0.75f, 0.01f));
}

TEST_CASE ("ADMOSCReceiver: /osd/obj/N/trajectory forwards paramName 'trajectory'", "[osc][per-object]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/4/trajectory", { 9.0f });

    REQUIRE (listener.paramCallCount == 1);
    CHECK (listener.lastParamObjectIndex == 3);
    CHECK (listener.lastParamName == "trajectory");
    CHECK_THAT (listener.lastParamValue, WithinAbs (9.0f, 0.5f));
}

TEST_CASE ("ADMOSCReceiver: /osd/obj/N/x,y,z partial Cartesian updates forward as object params", "[osc][per-object]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/1/x", { 0.5f });

    REQUIRE (listener.paramCallCount == 1);
    CHECK (listener.lastParamName == "x");
    CHECK_THAT (listener.lastParamValue, WithinAbs (0.5f, 0.01f));
}

// ============================================================================
// Section 4: /osd/global/ Global Params -- propertyName has the leading
// slash stripped (e.g. "delaytime" not "/delaytime").
// ============================================================================

TEST_CASE ("ADMOSCReceiver: /osd/global/delaytime forwards propertyName 'delaytime'", "[osc][global]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/global/delaytime", { 250.0f });

    REQUIRE (listener.globalCallCount == 1);
    CHECK (listener.lastGlobalProperty == "delaytime");
    CHECK_THAT (listener.lastGlobalValue, WithinAbs (250.0f, 1.0f));
}

TEST_CASE ("ADMOSCReceiver: /osd/global/tapazimuth forwards propertyName 'tapazimuth'", "[osc][global]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/global/tapazimuth", { 45.0f });

    REQUIRE (listener.globalCallCount == 1);
    CHECK (listener.lastGlobalProperty == "tapazimuth");
    CHECK_THAT (listener.lastGlobalValue, WithinAbs (45.0f, 0.01f));
}

// ============================================================================
// Section 5: Edge Cases & Boundary Conditions
// ============================================================================

TEST_CASE ("ADMOSCReceiver: Out-of-range object number is ignored", "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/adm/obj/0/azim", { 90.0f });    // 0 is below valid range (1-12)
    sendOSC (*receiver, "/adm/obj/13/azim", { 90.0f });   // 13 is above valid range (MAX_SOURCES=12)

    CHECK (listener.positionCallCount == 0);
}

TEST_CASE ("ADMOSCReceiver: Unknown /osd/obj/ property is silently ignored (no known object grammar matched)", "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/1/nonexistent", { 42.0f });

    CHECK (listener.positionCallCount == 0);
    CHECK (listener.paramCallCount == 0);
}

TEST_CASE ("ADMOSCReceiver: /osd/global/ forwards any property name -- consumer owns the known-property filter", "[osc][edge]")
{
    // The receiver decodes wire-format grammar only; deciding which global
    // property names are recognized (and silently ignoring unknown ones) is
    // the CONSUMER's responsibility, exactly mirroring the pre-move
    // OpenSpatialDelayProcessor::admGlobalParamReceived's exhaustive if-else
    // chain (Source/PluginProcessor.cpp) -- an unrecognized propertyName is
    // still forwarded here, just a no-op once it reaches the consumer.
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/global/nonexistent", { 42.0f });

    REQUIRE (listener.globalCallCount == 1);
    CHECK (listener.lastGlobalProperty == "nonexistent");
}

TEST_CASE ("ADMOSCReceiver: All 12 objects addressable", "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    for (int i = 1; i <= 12; ++i)
    {
        float targetAz = static_cast<float> (i * 15);
        sendOSC (*receiver, "/osd/obj/" + juce::String (i) + "/azim", { targetAz });
        CHECK (listener.lastPositionObjectIndex == i - 1);
        CHECK_THAT (listener.lastAz, WithinAbs (targetAz, 0.5f));
    }
    CHECK (listener.positionCallCount == 12);
}

TEST_CASE ("ADMOSCReceiver: Malformed address with no object number is ignored", "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    juce::OSCMessage msg ("/adm/obj/");  // no trailing number/property at all
    receiver->testProcessOSCMessage (msg);

    CHECK (listener.positionCallCount == 0);
}

TEST_CASE ("ADMOSCReceiver: removeListener stops further dispatch", "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/adm/obj/1/azim", { 10.0f });
    REQUIRE (listener.positionCallCount == 1);

    receiver->removeListener (&listener);
    sendOSC (*receiver, "/adm/obj/1/azim", { 20.0f });

    CHECK (listener.positionCallCount == 1);  // unchanged -- no further dispatch
}
