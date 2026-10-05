#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <SpatialCore/OSC/ADMOSCSender.h>
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include "../Support/FreeUdpPort.h"
#include "../Support/RecordingCapture.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

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

    // Queries (Phase 4 Plan 04-03): every admPositionQueried call, in order.
    std::vector<std::pair<int, ADMPositionQuery>> queries;
    int queryCallCount = 0;

    void admPositionQueried (int objectIndex, ADMPositionQuery kind) override
    {
        queries.emplace_back (objectIndex, kind);
        ++queryCallCount;
    }

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

TEST_CASE ("ADMOSCReceiver: an object number with anything but one or two digits is ignored (IN-03)",
           "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    // Each of these used to be read as object 2 (or 1) by getIntValue()'s leading-integer rule.
    const char* const bad[] = { "2x", "2.7", "+2", "-2", "x2", "0x2", "002", "4294967297", "" };
    for (const char* obj : bad)
    {
        DYNAMIC_SECTION ("/adm/obj/" << obj << "/azim")
        {
            sendOSC (*receiver, juce::String ("/adm/obj/") + obj + "/azim", { 45.0f });
            sendOSC (*receiver, juce::String ("/osd/obj/") + obj + "/azim", { 45.0f });
            CHECK (listener.positionCallCount == 0);
            CHECK (listener.queries.empty());
        }
    }

    // Plain numbers still work, with or without a leading zero.
    sendOSC (*receiver, "/adm/obj/2/azim", { 45.0f });
    CHECK (listener.lastPositionObjectIndex == 1);
    sendOSC (*receiver, "/adm/obj/02/azim", { 45.0f });
    CHECK (listener.lastPositionObjectIndex == 1);
    sendOSC (*receiver, "/adm/obj/12/azim", { 45.0f });
    CHECK (listener.lastPositionObjectIndex == 11);
    CHECK (listener.positionCallCount == 3);
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


// ============================================================================
// Position queries (Phase 4 Plan 04-03, D-08a, D-20). A message with no
// arguments to a position property is a query: the receiver reports it to the
// Listener, the consumer answers through ADMOSCSender::queueReply, and the
// reply leaves on the sender's next 30 Hz slot to its configured destination.
// ============================================================================

namespace
{
    using test::RecordingCapture;   // shared with the sender tests (IN-04)

    // The consumer's side of the contract: it owns the current position of every
    // object and answers a query from that state. The receiver stores nothing (DR-16).
    struct QueryGlue : public ADMOSCReceiver::Listener
    {
        explicit QueryGlue (ADMOSCSender& s) : sender (s) {}

        void admPositionReceived (int, float, float, float) override {}

        void admPositionQueried (int objectIndex, ADMPositionQuery kind) override
        {
            ++queriesSeen;
            if (objectIndex >= 0 && objectIndex < (int) MAX_SOURCES)
                sender.queueReply (objectIndex, kind, az[objectIndex], el[objectIndex], dist[objectIndex]);
        }

        ADMOSCSender& sender;
        float az[MAX_SOURCES] = {};
        float el[MAX_SOURCES] = {};
        float dist[MAX_SOURCES] = {};
        int queriesSeen = 0;
    };

    void pumpUntilCount (const int& counter, int expected)
    {
        for (int i = 0; i < 40 && counter < expected; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }
}

TEST_CASE ("ADM-OSC query: /adm/obj/4/xyz with no arguments over UDP is answered at the configured destination",
           "[osc][query][udp][tracer]")
{
    // Same rule as the route tracer: the GUI initialiser comes first and goes out of
    // scope last, or MessageLoopCallback delivery crashes in this console executable.
    juce::ScopedJuceInitialiser_GUI juceInit;

    // The device's return port, standing in for wherever the sender is configured to send.
    const int returnPort = test::findFreeUdpPort();
    const int queryPort = test::findFreeUdpPort();
    REQUIRE (returnPort > 0);
    REQUIRE (queryPort > 0);
    REQUIRE (returnPort != queryPort);

    juce::OSCReceiver deviceReturn;
    RecordingCapture capture (returnPort);
    REQUIRE (deviceReturn.connect (returnPort));
    deviceReturn.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", returnPort));

    QueryGlue glue (sender);
    glue.az[3] = 90.0f;   // address /adm/obj/4/ is object index 3
    glue.el[3] = 0.0f;
    glue.dist[3] = 0.5f;

    ADMOSCReceiver rx;
    rx.addListener (&glue);
    REQUIRE (rx.connect (queryPort));

    // The late-joining device: a plain OSC sender, no arguments.
    juce::OSCSender device;
    REQUIRE (device.connect ("127.0.0.1", queryPort));
    REQUIRE (device.send (juce::OSCMessage { juce::OSCAddressPattern { "/adm/obj/4/xyz" } }));

    pumpUntilCount (glue.queriesSeen, 1);
    REQUIRE (glue.queriesSeen == 1);

    // Every object disabled: no position traffic, only the reply. The first call
    // after connect() is a send slot.
    const float azs[MAX_SOURCES] = {}, els[MAX_SOURCES] = {}, dists[MAX_SOURCES] = {};
    const bool enabled[MAX_SOURCES] = {};
    sender.tick (azs, els, dists, enabled, (int) MAX_SOURCES, 0.0);

    REQUIRE (capture.settle() == 1);
    const auto reply = capture.at (0);
    CHECK (reply.address == "/adm/obj/4/xyz");
    REQUIRE (reply.argCount == 3);
    CHECK_THAT (reply.args[0], WithinAbs (-0.5f, 1.0e-4f));
    CHECK_THAT (reply.args[1], WithinAbs (0.0f, 1.0e-4f));
    CHECK_THAT (reply.args[2], WithinAbs (0.0f, 1.0e-4f));

    device.disconnect();
    rx.disconnect();
    rx.removeListener (&glue);
    deviceReturn.removeListener (&capture);
    sender.disconnect();
    deviceReturn.disconnect();
}

// ----------------------------------------------------------------------------
// Query grammar (D-08a): no arguments to a position property is a query.
// ----------------------------------------------------------------------------

TEST_CASE ("ADM-OSC query: every position property with no arguments is reported once with its kind",
           "[osc][query]")
{
    struct Row { const char* property; ADMPositionQuery kind; };
    const Row rows[] = { { "azim", ADMPositionQuery::azim }, { "elev", ADMPositionQuery::elev },
                         { "dist", ADMPositionQuery::dist }, { "aed",  ADMPositionQuery::aed },
                         { "xyz",  ADMPositionQuery::xyz } };

    for (const auto& row : rows)
    {
        DYNAMIC_SECTION ("/adm/obj/2/" << row.property)
        {
            RecordingListener listener;
            auto receiver = createTestReceiver (listener);

            sendOSC (*receiver, juce::String ("/adm/obj/2/") + row.property, {});

            REQUIRE (listener.queryCallCount == 1);
            CHECK (listener.queries[0].first == 1);   // 1-based address -> 0-based index
            CHECK (listener.queries[0].second == row.kind);
            CHECK (listener.positionCallCount == 0);
            CHECK (listener.paramCallCount == 0);
            CHECK (listener.globalCallCount == 0);
        }
    }
}

TEST_CASE ("ADM-OSC query: the /osd/obj/N/ alias of a position property is also a query",
           "[osc][query]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/2/aed", {});

    REQUIRE (listener.queryCallCount == 1);
    CHECK (listener.queries[0].first == 1);
    CHECK (listener.queries[0].second == ADMPositionQuery::aed);
    CHECK (listener.positionCallCount == 0);
}

TEST_CASE ("ADM-OSC query: a no-argument message to any other property or object is ignored",
           "[osc][query][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    sendOSC (*receiver, "/osd/obj/1/enabled", {});
    sendOSC (*receiver, "/osd/obj/1/doppler", {});
    sendOSC (*receiver, "/adm/obj/1/x", {});
    sendOSC (*receiver, "/adm/obj/13/aed", {});      // object 13 does not exist
    sendOSC (*receiver, "/osd/global/drywet", {});

    CHECK (listener.queryCallCount == 0);
    CHECK (listener.positionCallCount == 0);
    CHECK (listener.paramCallCount == 0);
    CHECK (listener.globalCallCount == 0);
}

// ----------------------------------------------------------------------------
// Input hardening (Phase 4 Plan 04-03, D-21, T-04-05, T-04-06). The datagrams are
// unauthenticated: a wrong-typed argument, a NaN or an out-of-range number must
// neither assert, nor reach the consumer as a position, nor hang a wrap loop.
// ----------------------------------------------------------------------------

namespace
{
    // One argument of any type, so the tests can send what a hostile or sloppy
    // device might.
    juce::OSCMessage oneArg (const juce::String& address, const juce::OSCArgument& arg)
    {
        juce::OSCMessage m (address);
        m.addArgument (arg);
        return m;
    }

    juce::OSCMessage threeFloats (const juce::String& address, float a, float b, float c)
    {
        juce::OSCMessage m (address);
        m.addFloat32 (a);
        m.addFloat32 (b);
        m.addFloat32 (c);
        return m;
    }
}

TEST_CASE ("ADMOSCReceiver: a wrong-typed argument is ignored, not read with the wrong getter",
           "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    // A string where a number belongs: previously getInt32() on a string, which is
    // 0 in Release and a JUCE assertion in Debug (Pitfall 5).
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/enabled", juce::OSCArgument (juce::String ("on"))));
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/trajectory", juce::OSCArgument (juce::String ("circle"))));
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/direction", juce::OSCArgument (juce::String ("fwd"))));
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/input", juce::OSCArgument (juce::String ("1"))));
    receiver->testProcessOSCMessage (oneArg ("/osd/global/drywet", juce::OSCArgument (juce::String ("0.5"))));
    // An int32 where only float32 is accepted (position axes, /doppler /pitch /speed).
    receiver->testProcessOSCMessage (oneArg ("/adm/obj/1/azim", juce::OSCArgument (90)));
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/doppler", juce::OSCArgument (1)));
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/pitch", juce::OSCArgument (1)));
    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/speed", juce::OSCArgument (1)));
    // A string in a position triple.
    juce::OSCMessage aed ("/adm/obj/1/aed");
    aed.addFloat32 (10.0f);
    aed.addString ("x");
    aed.addFloat32 (0.5f);
    receiver->testProcessOSCMessage (aed);

    CHECK (listener.paramCallCount == 0);
    CHECK (listener.globalCallCount == 0);
    CHECK (listener.positionCallCount == 0);
}

TEST_CASE ("ADMOSCReceiver: int32 is still accepted for the switch and index parameters",
           "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    receiver->testProcessOSCMessage (oneArg ("/osd/obj/1/enabled", juce::OSCArgument (1)));
    REQUIRE (listener.paramCallCount == 1);
    CHECK (listener.lastParamName == "enabled");
    CHECK (listener.lastParamValue == 1.0f);

    receiver->testProcessOSCMessage (oneArg ("/osd/obj/2/trajectory", juce::OSCArgument (3)));
    CHECK (listener.lastParamName == "trajectory");
    CHECK (listener.lastParamValue == 3.0f);

    receiver->testProcessOSCMessage (oneArg ("/osd/obj/2/direction", juce::OSCArgument (-1)));
    CHECK (listener.lastParamName == "direction");
    CHECK (listener.lastParamValue == -1.0f);

    receiver->testProcessOSCMessage (oneArg ("/osd/obj/2/input", juce::OSCArgument (4)));
    CHECK (listener.lastParamName == "input");
    CHECK (listener.lastParamValue == 4.0f);

    receiver->testProcessOSCMessage (oneArg ("/osd/global/delaytime", juce::OSCArgument (2)));
    REQUIRE (listener.globalCallCount == 1);
    CHECK (listener.lastGlobalProperty == "delaytime");
    CHECK (listener.lastGlobalValue == 2.0f);

    // float32 keeps working for the same properties.
    sendOSC (*receiver, "/osd/obj/1/enabled", { 0.0f });
    CHECK (listener.lastParamValue == 0.0f);
    sendOSC (*receiver, "/osd/global/drywet", { 0.25f });
    CHECK (listener.lastGlobalValue == 0.25f);
}

TEST_CASE ("ADMOSCReceiver: NaN and infinity anywhere in a message drop the whole message",
           "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", nan, 0.0f, 0.5f));
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 10.0f, nan, 0.5f));
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 10.0f, 0.0f, inf));
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", -inf, 0.0f, 0.5f));
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/xyz", 0.1f, nan, 0.2f));
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/xyz", inf, 0.1f, 0.2f));
    sendOSC (*receiver, "/adm/obj/1/azim", { nan });
    sendOSC (*receiver, "/adm/obj/1/azim", { inf });
    sendOSC (*receiver, "/adm/obj/1/elev", { nan });
    sendOSC (*receiver, "/adm/obj/1/dist", { -inf });
    sendOSC (*receiver, "/adm/obj/1/x", { nan });
    sendOSC (*receiver, "/osd/obj/1/speed", { inf });
    sendOSC (*receiver, "/osd/obj/1/doppler", { nan });
    sendOSC (*receiver, "/osd/obj/1/enabled", { nan });
    sendOSC (*receiver, "/osd/global/drywet", { inf });

    CHECK (listener.positionCallCount == 0);
    CHECK (listener.paramCallCount == 0);
    CHECK (listener.globalCallCount == 0);
}

TEST_CASE ("ADMOSCReceiver: elevation, distance and partial cartesian axes are clamped to their range",
           "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 30.0f, 120.0f, 0.5f));
    CHECK (listener.lastEl == 90.0f);
    CHECK (listener.lastAz == 30.0f);

    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 30.0f, -120.0f, 0.5f));
    CHECK (listener.lastEl == -90.0f);

    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 30.0f, 0.0f, 5.0f));
    CHECK (listener.lastDist == 1.0f);

    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 30.0f, 0.0f, -1.0f));
    CHECK (listener.lastDist == 0.0f);

    sendOSC (*receiver, "/adm/obj/1/elev", { 500.0f });
    CHECK (listener.lastEl == 90.0f);

    sendOSC (*receiver, "/adm/obj/1/dist", { 9.0f });
    CHECK (listener.lastDist == 1.0f);

    sendOSC (*receiver, "/adm/obj/1/x", { 7.0f });
    CHECK (listener.lastParamName == "x");
    CHECK (listener.lastParamValue == 1.0f);

    sendOSC (*receiver, "/adm/obj/1/y", { -7.0f });
    CHECK (listener.lastParamName == "y");
    CHECK (listener.lastParamValue == -1.0f);
}

TEST_CASE ("ADMOSCReceiver: a huge azimuth is wrapped once and the result is safe for wrapAzimuth",
           "[osc][edge]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    const float inputs[] = { 181.0f, -181.0f, 540.0f, 1.0e10f, -1.0e10f, 1.0e30f };
    for (float az : inputs)
    {
        DYNAMIC_SECTION ("azimuth " << az)
        {
            listener.positionCallCount = 0;
            receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", az, 0.0f, 0.5f));
            REQUIRE (listener.positionCallCount == 1);
            CHECK (std::isfinite (listener.lastAz));
            CHECK (listener.lastAz >= -180.0f);
            CHECK (listener.lastAz <= 180.0f);

            // The consumer's subtract-360 loop returns at once on the forwarded value
            // (on the raw 1e10 it never returns: 1e10 - 360 == 1e10 in float).
            const float wrapped = wrapAzimuth (listener.lastAz);
            CHECK (wrapped == listener.lastAz);

            listener.positionCallCount = 0;
            sendOSC (*receiver, "/adm/obj/1/azim", { az });
            REQUIRE (listener.positionCallCount == 1);
            CHECK (listener.lastAz >= -180.0f);
            CHECK (listener.lastAz <= 180.0f);
        }
    }

    // Everything huge at once: finite, in range, distance and elevation at their limits.
    listener.positionCallCount = 0;
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", 1.0e30f, 1.0e30f, 1.0e30f));
    REQUIRE (listener.positionCallCount == 1);
    CHECK (std::isfinite (listener.lastAz));
    CHECK (listener.lastAz >= -180.0f);
    CHECK (listener.lastAz <= 180.0f);
    CHECK (listener.lastEl == 90.0f);
    CHECK (listener.lastDist == 1.0f);

    // A huge cartesian triple still comes out finite and in range.
    listener.positionCallCount = 0;
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/xyz", 1.0e30f, -1.0e30f, 1.0e30f));
    REQUIRE (listener.positionCallCount == 1);
    CHECK (std::isfinite (listener.lastAz));
    CHECK (std::isfinite (listener.lastEl));
    CHECK (listener.lastDist == 1.0f);

    // WR-04: a component that overflows float when squared must not flatten the elevation.
    // (1e30, 0, 1e30) points 45 degrees up and to the left (x is -azimuth).
    listener.positionCallCount = 0;
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/xyz", 1.0e30f, 0.0f, 1.0e30f));
    REQUIRE (listener.positionCallCount == 1);
    CHECK_THAT (listener.lastAz, Catch::Matchers::WithinAbs (-90.0f, 1.0e-3f));
    CHECK_THAT (listener.lastEl, Catch::Matchers::WithinAbs (45.0f, 1.0e-3f));
    CHECK (listener.lastDist == 1.0f);

    // The same direction written with a small, in-range scale gives the same angles.
    listener.positionCallCount = 0;
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/xyz", 0.5f, 0.5f, 0.5f));
    REQUIRE (listener.positionCallCount == 1);
    const float smallAz = listener.lastAz, smallEl = listener.lastEl;
    listener.positionCallCount = 0;
    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/xyz", 3.0e30f, 3.0e30f, 3.0e30f));
    REQUIRE (listener.positionCallCount == 1);
    CHECK_THAT (listener.lastAz, Catch::Matchers::WithinAbs (smallAz, 1.0e-3f));
    CHECK_THAT (listener.lastEl, Catch::Matchers::WithinAbs (smallEl, 1.0e-3f));
}

TEST_CASE ("ADMOSCReceiver: in-range float32 messages from a compliant sender forward bit for bit",
           "[osc][edge][compliant]")
{
    RecordingListener listener;
    auto receiver = createTestReceiver (listener);

    // The BASE conversion, copied so the comparison does not depend on the code under test.
    const auto baseXyz = [] (float x, float y, float z, float& az, float& el, float& d)
    {
        constexpr float kPi = 3.14159265358979323846f;
        az = std::atan2 (-x, y) * (180.0f / kPi);
        const float r = std::sqrt (x * x + y * y);
        el = std::atan2 (z, r) * (180.0f / kPi);
        d = juce::jlimit (0.0f, 1.0f, std::sqrt (x * x + y * y + z * z));
    };

    receiver->testProcessOSCMessage (threeFloats ("/adm/obj/1/aed", -179.5f, -89.9f, 0.999f));
    REQUIRE (listener.positionCallCount == 1);
    CHECK (listener.lastAz == -179.5f);
    CHECK (listener.lastEl == -89.9f);
    CHECK (listener.lastDist == 0.999f);

    sendOSC (*receiver, "/adm/obj/1/azim", { 180.0f });
    CHECK (listener.lastAz == 180.0f);
    CHECK (std::isnan (listener.lastEl));
    CHECK (std::isnan (listener.lastDist));

    sendOSC (*receiver, "/adm/obj/1/azim", { -180.0f });
    CHECK (listener.lastAz == -180.0f);

    sendOSC (*receiver, "/adm/obj/1/elev", { 45.25f });
    CHECK (listener.lastEl == 45.25f);
    CHECK (std::isnan (listener.lastAz));

    sendOSC (*receiver, "/adm/obj/1/dist", { 0.0f });
    CHECK (listener.lastDist == 0.0f);

    float az, el, d;
    baseXyz (0.3f, -0.4f, 0.5f, az, el, d);
    sendOSC (*receiver, "/adm/obj/1/xyz", { 0.3f, -0.4f, 0.5f });
    CHECK (listener.lastAz == az);
    CHECK (listener.lastEl == el);
    CHECK (listener.lastDist == d);

    sendOSC (*receiver, "/adm/obj/1/y", { -0.4f });
    CHECK (listener.lastParamName == "y");
    CHECK (listener.lastParamValue == -0.4f);

    sendOSC (*receiver, "/osd/obj/3/speed", { 0.37f });
    CHECK (listener.lastParamObjectIndex == 2);
    CHECK (listener.lastParamValue == 0.37f);
}
