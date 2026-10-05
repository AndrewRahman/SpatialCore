#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <SpatialCore/OSC/ADMOSCReceiver.h>   // the xyz round-trip test feeds a reply back into the receiver
#include <SpatialCore/OSC/ADMOSCSender.h>
#include "../Support/FreeUdpPort.h"
#include "../Support/RecordingCapture.h"
#include <juce_osc/juce_osc.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

using namespace spatialcore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// ADMOSCSender wire-format tests -- verifies the position broadcast
// (/adm/obj/N/aed) matches the pre-move OSD send format exactly (Phase 8 Plan
// 08-05, CORE-05): 1-based object numbering, three float32 args (azimuth,
// elevation, distance) in that order. A real loopback juce::OSCReceiver
// (RealtimeCallback -- dispatches synchronously off the network thread, no
// JUCE MessageManager/message loop required in the Catch2 console runner)
// captures what ADMOSCSender actually puts on the wire.
// ============================================================================

namespace
{
    using test::RecordingCapture;   // shared with the receiver tests (IN-04)
}

TEST_CASE ("ADMOSCSender: sendPosition emits /adm/obj/N/aed with 1-based object numbering", "[osc][send]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    // objectIndex 0 (0-based) -> wire address /adm/obj/1/aed (1-based)
    sender.sendPosition (0, -45.0f, 30.0f, 0.6f);

    REQUIRE (capture.settle() == 1);
    const auto m = capture.last();
    CHECK (m.address == "/adm/obj/1/aed");
    REQUIRE (m.argCount == 3);
    CHECK_THAT (m.args[0], WithinAbs (-45.0f, 0.5f));
    CHECK_THAT (m.args[1], WithinAbs (30.0f, 0.5f));
    CHECK_THAT (m.args[2], WithinAbs (0.6f, 0.01f));

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: sendPosition ignores calls while disconnected", "[osc][send][edge]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    CHECK_FALSE (sender.isConnected());

    // Not connected -- must be a silent no-op (matches pre-move OSD behavior
    // of gating all sends on oscSendConnected).
    sender.sendPosition (0, 10.0f, 10.0f, 1.0f);

    // settle() proves everything sent so far has been delivered, so zero is a fact, not a timeout.
    CHECK (capture.settle() == 0);

    receiver.removeListener (&capture);
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: sendPosition ignores out-of-range object index", "[osc][send][edge]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    sender.sendPosition (-1, 10.0f, 10.0f, 1.0f);   // below range
    sender.sendPosition (MAX_SOURCES, 10.0f, 10.0f, 1.0f); // at/above range (0-based, so == out of range)

    CHECK (capture.settle() == 0);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

// ============================================================================
// Self-clocked 30 Hz schedule (Phase 4 Plan 04-02, D-07/D-08c/D-10). The
// six-argument tick takes the caller's clock, so the tests drive simulated time.
// ============================================================================

TEST_CASE ("ADMOSCSender: a 60 Hz caller moving an object sends 30 messages per second, none while still",
           "[osc][send][rate][static][tracer]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    float az = 0.0f, el = 0.0f, dist = 0.5f;
    bool enabled = true;

    // 3 simulated seconds at 60 Hz, azimuth rising 1 degree per call.
    for (int second = 0; second < 3; ++second)
    {
        for (int k = second * 60; k < (second + 1) * 60; ++k)
        {
            az = (float) (k + 1);
            sender.tick (&az, &el, &dist, &enabled, 1, k / 60.0);
        }
        capture.settle();
    }

    const int moving = capture.settle();
    CHECK (moving >= 87);
    CHECK (moving <= 93);

    // The last move landed on a non-slot call, so the next slot flushes it
    // (a change is sent at the next slot). Let that settle, then baseline.
    for (int k = 180; k < 186; ++k)
        sender.tick (&az, &el, &dist, &enabled, 1, k / 60.0);
    const int settled = capture.settle();
    CHECK (settled - moving <= 1);

    // 2 more simulated seconds, position unchanged: nothing is sent.
    for (int k = 186; k < 306; ++k)
        sender.tick (&az, &el, &dist, &enabled, 1, k / 60.0);

    CHECK (capture.settle() == settled);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

namespace
{
    // Drives one moving object at callerHz for `seconds` simulated seconds with
    // +-jitterSeconds of timing noise, waiting for the wire to go quiet after
    // each simulated second so few datagrams are ever in flight.
    void runMovingCaller (ADMOSCSender& sender, RecordingCapture& capture,
                          double callerHz, int seconds, double jitterSeconds,
                          double startSeconds = 0.0)
    {
        juce::Random rng (42);
        const double period = 1.0 / callerHz;
        const int callsPerSecond = (int) std::lround (callerHz);
        float az = 0.0f;
        const float el = 0.0f, dist = 0.5f;
        const bool enabled = true;

        for (int s = 0; s < seconds; ++s)
        {
            for (int c = 0; c < callsPerSecond; ++c)
            {
                const int k = s * callsPerSecond + c;
                const double jitter = jitterSeconds > 0.0
                    ? (rng.nextDouble() * 2.0 - 1.0) * jitterSeconds : 0.0;
                az += 1.0f;
                sender.tick (&az, &el, &dist, &enabled, 1,
                             startSeconds + k * period + jitter);
            }
            capture.settle();
        }
    }

    void tickObjects (ADMOSCSender& sender, double t, const float* az,
                      const bool* enabled, int n)
    {
        const float el[MAX_SOURCES] = {};
        const float dist[MAX_SOURCES] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        sender.tick (az, el, dist, enabled, n, t);
    }
}

TEST_CASE ("ADMOSCSender: any caller rate from 30 to 120 Hz gets 30 messages per second with jitter",
           "[osc][send][rate]")
{
    const int hz = GENERATE (120, 60, 50, 30);
    CAPTURE (hz);
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);

    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    runMovingCaller (sender, capture, (double) hz, 10, 0.002);

    const int total = capture.settle();
    INFO ("messages over 10 simulated seconds at " << hz << " Hz: " << total);
    CHECK (total >= 290);
    CHECK (total <= 310);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: a caller at exactly 1/60 s spacing gets 30 messages per second",
           "[osc][send][rate]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    runMovingCaller (sender, capture, 60.0, 10, 0.0);

    const int total = capture.settle();
    INFO ("messages over 10 simulated seconds at exact 1/60 s: " << total);
    CHECK (total >= 290);
    CHECK (total <= 310);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: the five-argument tick on the wall clock sends about 30 messages in a second",
           "[osc][send][rate][realtime]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    float az = 0.0f;
    const float el = 0.0f, dist = 0.5f;
    const bool enabled = true;

    // About one second of 60 Hz wall-clock calls. A loaded machine stretches the
    // sleeps, so the rate is judged against the elapsed time actually measured.
    const auto start = std::chrono::steady_clock::now();
    auto lastCall = start;
    for (int i = 0; i < 60; ++i)
    {
        az += 1.0f;
        sender.tick (&az, &el, &dist, &enabled, 1);
        lastCall = std::chrono::steady_clock::now();
        juce::Thread::sleep (16);
    }
    const double elapsed = std::chrono::duration<double> (lastCall - start).count();

    const int total = capture.settle();
    INFO ("messages from 60 wall-clock calls: " << total << " over " << elapsed << " s");
    CHECK (elapsed >= 0.9);
    CHECK ((double) total >= 30.0 * elapsed - 5.0);
    CHECK ((double) total <= 30.0 * elapsed + 5.0);
    if (elapsed < 1.1)
    {
        CHECK (total >= 25);
        CHECK (total <= 35);
    }

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: after a one second stall the next second is 30 messages, not a burst",
           "[osc][send][rate][gap]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    runMovingCaller (sender, capture, 60.0, 1, 0.0);
    const int beforeStall = capture.settle();

    // No calls for one second, then one second of 60 Hz calls with movement.
    float az = 1000.0f;
    const float el = 0.0f, dist = 0.5f;
    const bool enabled = true;
    for (int k = 120; k < 180; ++k)
    {
        az += 1.0f;
        sender.tick (&az, &el, &dist, &enabled, 1, k / 60.0);
    }

    const int afterStall = capture.settle();
    INFO ("messages in the second after the stall: " << (afterStall - beforeStall));
    CHECK (afterStall - beforeStall >= 29);
    CHECK (afterStall - beforeStall <= 31);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: an object's first position is sent even at exactly (0, 0, 0)",
           "[osc][send][first]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    const float az = 0.0f, el = 0.0f, dist = 0.0f;
    const bool enabled = true;
    sender.tick (&az, &el, &dist, &enabled, 1, 0.0);

    REQUIRE (capture.settle() == 1);
    const auto m = capture.last();
    CHECK (m.address == "/adm/obj/1/aed");
    REQUIRE (m.argCount == 3);
    CHECK (m.args[0] == 0.0f);
    CHECK (m.args[1] == 0.0f);
    CHECK (m.args[2] == 0.0f);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: a change of exactly the dead-band is not sent, twice the dead-band is",
           "[osc][send][deadband]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    const float el = 0.0f, dist = 0.0f;
    const bool enabled = true;

    float az = 0.0f;
    sender.tick (&az, &el, &dist, &enabled, 1, 0.0);          // first send
    REQUIRE (capture.settle() == 1);

    // IN-08: the dead-band comes from the class, not a number restated here.
    const float angleBand = ADMOSCSender::kAngleDeadBandDeg;
    const float distBand  = ADMOSCSender::kDistanceDeadBand;

    az = angleBand;
    sender.tick (&az, &el, &dist, &enabled, 1, 1.0 / 30.0);   // not greater than the dead-band
    CHECK (capture.settle() == 1);

    az = 2.0f * angleBand;
    sender.tick (&az, &el, &dist, &enabled, 1, 2.0 / 30.0);   // greater
    REQUIRE (capture.settle() == 2);
    CHECK_THAT (capture.last().args[0], WithinAbs (2.0f * angleBand, 1.0e-6f));

    // Distance has its own, smaller dead-band.
    float movedDist = distBand;
    sender.tick (&az, &el, &movedDist, &enabled, 1, 3.0 / 30.0);
    CHECK (capture.settle() == 2);

    movedDist = 2.0f * distBand;
    sender.tick (&az, &el, &movedDist, &enabled, 1, 4.0 / 30.0);
    REQUIRE (capture.settle() == 3);
    CHECK_THAT (capture.last().args[2], WithinAbs (2.0f * distBand, 1.0e-6f));

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: a non-finite position is never sent and does not silence the object (WR-01)",
           "[osc][send][deadband][nan]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    const bool enabled = true;
    float az = 10.0f, el = 0.0f, dist = 0.5f;
    sender.tick (&az, &el, &dist, &enabled, 1, 0.0);           // first send
    REQUIRE (capture.settle() == 1);

    // One NaN frame on each axis in turn, then +-infinity: nothing reaches the wire.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    float badAz = nan, badEl = nan, badDist = nan;
    sender.tick (&badAz, &el, &dist, &enabled, 1, 1.0 / 30.0);
    sender.tick (&az, &badEl, &dist, &enabled, 1, 2.0 / 30.0);
    sender.tick (&az, &el, &badDist, &enabled, 1, 3.0 / 30.0);
    badAz = inf; badEl = -inf;
    sender.tick (&badAz, &el, &dist, &enabled, 1, 4.0 / 30.0);
    sender.tick (&az, &badEl, &dist, &enabled, 1, 5.0 / 30.0);
    CHECK (capture.settle() == 1);

    // The next finite, moved position is sent: the dead-band reference was not poisoned.
    az = 25.0f;
    sender.tick (&az, &el, &dist, &enabled, 1, 6.0 / 30.0);
    REQUIRE (capture.settle() == 2);
    CHECK_THAT (capture.last().args[0], WithinAbs (25.0f, 1.0e-6f));

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: still objects are silent and one moved object sends one message",
           "[osc][send][static]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    const bool enabled[3] = { true, true, true };
    float az[3] = { 10.0f, 20.0f, 30.0f };
    tickObjects (sender, 0.0, az, enabled, 3);                 // first send, 3 messages
    az[0] = 40.0f; az[1] = 50.0f; az[2] = 60.0f;
    tickObjects (sender, 1.0 / 30.0, az, enabled, 3);          // moved, 3 messages
    REQUIRE (capture.settle() == 6);

    for (int k = 4; k < 124; ++k)                              // 2 simulated seconds still
        tickObjects (sender, k / 60.0, az, enabled, 3);
    CHECK (capture.settle() == 6);

    az[2] = 90.0f;                                             // move object index 2 only
    tickObjects (sender, 124 / 60.0, az, enabled, 3);
    tickObjects (sender, 125 / 60.0, az, enabled, 3);
    tickObjects (sender, 126 / 60.0, az, enabled, 3);

    CHECK (capture.settle() == 7);
    CHECK (capture.countFor ("/adm/obj/3/aed") == 3);
    CHECK (capture.last().address == "/adm/obj/3/aed");
    CHECK_THAT (capture.last().args[0], WithinAbs (90.0f, 1.0e-6f));

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: connect and reconnect send every enabled object once, never a disabled one",
           "[osc][send][connect]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    const bool enabled[4] = { true, true, true, false };
    const float az[4] = { 10.0f, 20.0f, 30.0f, 40.0f };

    tickObjects (sender, 0.0, az, enabled, 4);
    REQUIRE (capture.settle() == 3);

    for (int k = 1; k < 61; ++k)                               // a still second
        tickObjects (sender, k / 60.0, az, enabled, 4);
    CHECK (capture.settle() == 3);

    sender.disconnect();
    REQUIRE (sender.connect ("127.0.0.1", port));

    tickObjects (sender, 2.0, az, enabled, 4);                 // nothing moved
    CHECK (capture.settle() == 6);
    CHECK (capture.countFor ("/adm/obj/1/aed") == 2);
    CHECK (capture.countFor ("/adm/obj/2/aed") == 2);
    CHECK (capture.countFor ("/adm/obj/3/aed") == 2);
    CHECK (capture.countFor ("/adm/obj/4/aed") == 0);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: an object enabled again is sent once, and nothing is sent while it is disabled",
           "[osc][send][reenable]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    const float az[2] = { 10.0f, 20.0f };
    const bool both[2]    = { true, true };
    const bool oneOff[2]  = { true, false };

    tickObjects (sender, 0.0, az, both, 2);                    // both sent
    REQUIRE (capture.settle() == 2);

    // Disabled for one call between slots, enabled again at the same position.
    tickObjects (sender, 1.0 / 60.0, az, oneOff, 2);
    tickObjects (sender, 2.0 / 60.0, az, both, 2);             // slot
    CHECK (capture.settle() == 3);
    CHECK (capture.last().address == "/adm/obj/2/aed");

    // Disabled across a whole slot: nothing is sent for it.
    tickObjects (sender, 3.0 / 60.0, az, oneOff, 2);
    tickObjects (sender, 4.0 / 60.0, az, oneOff, 2);           // slot, still disabled
    CHECK (capture.settle() == 3);

    tickObjects (sender, 5.0 / 60.0, az, both, 2);
    tickObjects (sender, 6.0 / 60.0, az, both, 2);             // slot
    CHECK (capture.settle() == 4);
    CHECK (capture.countFor ("/adm/obj/2/aed") == 3);
    CHECK (capture.countFor ("/adm/obj/1/aed") == 1);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

// ============================================================================
// Query replies (Phase 4 Plan 04-03, D-08a, D-20, T-04-07). queueReply holds one
// pending reply per (object, kind); the 30 Hz slot flushes them in object order,
// then azim, elev, dist, aed, xyz order. Ports 9752 and up.
// ============================================================================

namespace
{
    // Calls tick with every object disabled (no position traffic) at time t.
    void tickSilent (ADMOSCSender& sender, double t)
    {
        const float zeros[MAX_SOURCES] = {};
        const bool off[MAX_SOURCES] = {};
        sender.tick (zeros, zeros, zeros, off, (int) MAX_SOURCES, t);
    }

    struct PositionSink : public ADMOSCReceiver::Listener
    {
        void admPositionReceived (int objectIndex, float az, float el, float d) override
        {
            index = objectIndex; azimuth = az; elevation = el; distance = d; ++calls;
        }
        int index = -1, calls = 0;
        float azimuth = NAN, elevation = NAN, distance = NAN;
    };
}

TEST_CASE ("ADMOSCSender: queued replies go out in object then kind order with the right formats",
           "[osc][send][query]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    // Queued out of order on purpose: xyz first, azim last; objects 4 then 0.
    sender.queueReply (4, ADMPositionQuery::aed, 5.0f, 6.0f, 0.7f);
    sender.queueReply (0, ADMPositionQuery::xyz, 30.0f, 10.0f, 0.5f);
    sender.queueReply (0, ADMPositionQuery::aed, 30.0f, 10.0f, 0.5f);
    sender.queueReply (0, ADMPositionQuery::dist, 30.0f, 10.0f, 0.5f);
    sender.queueReply (0, ADMPositionQuery::elev, 30.0f, 10.0f, 0.5f);
    sender.queueReply (0, ADMPositionQuery::azim, 30.0f, 10.0f, 0.5f);
    tickSilent (sender, 0.0);

    REQUIRE (capture.settle() == 6);

    const char* addresses[] = { "/adm/obj/1/azim", "/adm/obj/1/elev", "/adm/obj/1/dist",
                                "/adm/obj/1/aed",  "/adm/obj/1/xyz",  "/adm/obj/5/aed" };
    const int argCounts[] = { 1, 1, 1, 3, 3, 3 };
    for (int i = 0; i < 6; ++i)
    {
        INFO ("message " << i);
        const auto e = capture.at (i);
        CHECK (e.address == addresses[i]);
        CHECK (e.argCount == argCounts[i]);
    }

    CHECK_THAT (capture.at (0).args[0], WithinAbs (30.0f, 1.0e-4f));
    CHECK_THAT (capture.at (1).args[0], WithinAbs (10.0f, 1.0e-4f));
    CHECK_THAT (capture.at (2).args[0], WithinAbs (0.5f, 1.0e-4f));
    CHECK_THAT (capture.at (3).args[0], WithinAbs (30.0f, 1.0e-4f));
    CHECK_THAT (capture.at (3).args[1], WithinAbs (10.0f, 1.0e-4f));
    CHECK_THAT (capture.at (3).args[2], WithinAbs (0.5f, 1.0e-4f));

    // x = -d cos(el) sin(az), y = d cos(el) cos(az), z = d sin(el)
    const float el = 10.0f * 3.14159265f / 180.0f, az = 30.0f * 3.14159265f / 180.0f;
    CHECK_THAT (capture.at (4).args[0], WithinAbs (-0.5f * std::cos (el) * std::sin (az), 1.0e-4f));
    CHECK_THAT (capture.at (4).args[1], WithinAbs ( 0.5f * std::cos (el) * std::cos (az), 1.0e-4f));
    CHECK_THAT (capture.at (4).args[2], WithinAbs ( 0.5f * std::sin (el), 1.0e-4f));

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: 1000 queries for one (object, kind) before a slot make one reply with the last value",
           "[osc][send][query]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    for (int i = 0; i < 1000; ++i)
        sender.queueReply (2, ADMPositionQuery::aed, (float) i * 0.1f, 0.0f, 0.5f);
    tickSilent (sender, 0.0);

    REQUIRE (capture.settle() == 1);
    CHECK (capture.at (0).address == "/adm/obj/3/aed");
    CHECK_THAT (capture.at (0).args[0], WithinAbs (99.9f, 1.0e-3f));

    // The reply was consumed: the next slot sends nothing.
    tickSilent (sender, 1.0 / 30.0);
    CHECK (capture.settle() == 1);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: a reply is held until the next slot and ignored while disconnected or for bad input",
           "[osc][send][query]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;

    // Queued while disconnected: dropped, so a later connect sends nothing.
    sender.queueReply (0, ADMPositionQuery::aed, 1.0f, 2.0f, 0.5f);
    REQUIRE (sender.connect ("127.0.0.1", port));
    tickSilent (sender, 0.0);
    CHECK (capture.settle() == 0);

    // Bad input is dropped: out-of-range index, non-finite value, unknown kind.
    sender.queueReply (-1, ADMPositionQuery::aed, 1.0f, 2.0f, 0.5f);
    sender.queueReply ((int) MAX_SOURCES, ADMPositionQuery::aed, 1.0f, 2.0f, 0.5f);
    sender.queueReply (0, ADMPositionQuery::aed, NAN, 2.0f, 0.5f);
    sender.queueReply (0, ADMPositionQuery::aed, 1.0f, INFINITY, 0.5f);
    sender.queueReply (0, static_cast<ADMPositionQuery> (9), 1.0f, 2.0f, 0.5f);
    tickSilent (sender, 1.0 / 30.0);
    CHECK (capture.settle() == 0);

    // A valid reply queued between slots waits for the next slot.
    sender.queueReply (1, ADMPositionQuery::azim, 45.0f, 0.0f, 0.5f);
    tickSilent (sender, 1.0 / 30.0 + 0.010);
    CHECK (capture.settle() == 0);
    tickSilent (sender, 2.0 / 30.0);
    REQUIRE (capture.settle() == 1);
    CHECK (capture.at (0).address == "/adm/obj/2/azim");

    // Disconnect drops a pending reply: it must not appear after a reconnect.
    sender.queueReply (1, ADMPositionQuery::azim, 45.0f, 0.0f, 0.5f);
    sender.disconnect();
    REQUIRE (sender.connect ("127.0.0.1", port));
    tickSilent (sender, 10.0);
    CHECK (capture.settle() == 1);

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: a reply neither suppresses nor triggers a position send",
           "[osc][send][query]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    float az[MAX_SOURCES] = {}, el[MAX_SOURCES] = {}, dist[MAX_SOURCES] = {};
    bool enabled[MAX_SOURCES] = {};
    az[0] = 20.0f; dist[0] = 0.5f; enabled[0] = true;

    // First slot: the first position goes out once.
    sender.tick (az, el, dist, enabled, (int) MAX_SOURCES, 0.0);
    REQUIRE (capture.settle() == 1);
    CHECK (capture.at (0).address == "/adm/obj/1/aed");

    // Object still, reply queued: only the reply goes out at the next slot.
    sender.queueReply (0, ADMPositionQuery::xyz, 20.0f, 0.0f, 0.5f);
    sender.tick (az, el, dist, enabled, (int) MAX_SOURCES, 1.0 / 30.0);
    REQUIRE (capture.settle() == 2);
    CHECK (capture.at (1).address == "/adm/obj/1/xyz");

    // The reply did not reset the dead-band reference: still no position send.
    sender.tick (az, el, dist, enabled, (int) MAX_SOURCES, 2.0 / 30.0);
    CHECK (capture.settle() == 2);

    // Object moved and a reply queued: both go out, position first.
    az[0] = 40.0f;
    sender.queueReply (0, ADMPositionQuery::azim, 40.0f, 0.0f, 0.5f);
    sender.tick (az, el, dist, enabled, (int) MAX_SOURCES, 3.0 / 30.0);
    REQUIRE (capture.settle() == 4);
    CHECK (capture.at (2).address == "/adm/obj/1/aed");
    CHECK (capture.at (3).address == "/adm/obj/1/azim");

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: an xyz reply fed back into ADMOSCReceiver reproduces the position",
           "[osc][send][query]")
{
    const int port = test::findFreeUdpPort();
    REQUIRE (port > 0);
    juce::OSCReceiver receiver;
    RecordingCapture capture (port);
    REQUIRE (receiver.connect (port));
    receiver.addListener (&capture);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", port));

    sender.queueReply (0, ADMPositionQuery::xyz, 120.0f, -20.0f, 0.7f);
    tickSilent (sender, 0.0);
    REQUIRE (capture.settle() == 1);
    const auto reply = capture.at (0);
    REQUIRE (reply.argCount == 3);

    PositionSink sink;
    ADMOSCReceiver rx;
    rx.addListener (&sink);
    juce::OSCMessage back (reply.address);
    for (int i = 0; i < 3; ++i)
        back.addFloat32 (reply.args[i]);
    rx.testProcessOSCMessage (back);
    rx.removeListener (&sink);

    REQUIRE (sink.calls == 1);
    CHECK (sink.index == 0);
    CHECK_THAT (sink.azimuth,   WithinAbs (120.0f, 1.0e-3f));
    CHECK_THAT (sink.elevation, WithinAbs (-20.0f, 1.0e-3f));
    CHECK_THAT (sink.distance,  WithinAbs (0.7f, 1.0e-3f));

    receiver.removeListener (&capture);
    sender.disconnect();
    receiver.disconnect();
}
