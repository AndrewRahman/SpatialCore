#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/OSC/ADMOSCSender.h>
#include <juce_osc/juce_osc.h>
#include <atomic>
#include <chrono>
#include <thread>

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
    // Distinct port per test case to avoid cross-test interference if a
    // prior test's socket teardown races with the next test's bind.
    constexpr int kTestSenderPort = 9700;

    struct CapturingListener : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
    {
        std::atomic<int> messageCount { 0 };
        juce::String lastAddress;
        float lastArgs[3] = {};
        int lastArgCount = 0;

        void oscMessageReceived (const juce::OSCMessage& message) override
        {
            lastAddress = message.getAddressPattern().toString();
            lastArgCount = message.size();
            for (int i = 0; i < message.size() && i < 3; ++i)
                lastArgs[i] = message[i].getFloat32();
            ++messageCount;
        }
    };

    // Poll briefly for the loopback message to arrive -- network delivery,
    // even on localhost, is asynchronous relative to the sending thread.
    bool waitForMessage (std::atomic<int>& counter, int expectedCount,
                         int timeoutMs = 2000)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (counter.load() >= expectedCount)
                return true;
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        return counter.load() >= expectedCount;
    }
}

TEST_CASE ("ADMOSCSender: sendPosition emits /adm/obj/N/aed with 1-based object numbering", "[osc][send]")
{
    juce::OSCReceiver receiver;
    CapturingListener listener;
    REQUIRE (receiver.connect (kTestSenderPort));
    receiver.addListener (&listener);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", kTestSenderPort));

    // objectIndex 0 (0-based) -> wire address /adm/obj/1/aed (1-based)
    sender.sendPosition (0, -45.0f, 30.0f, 0.6f);

    REQUIRE (waitForMessage (listener.messageCount, 1));
    CHECK (listener.lastAddress == "/adm/obj/1/aed");
    REQUIRE (listener.lastArgCount == 3);
    CHECK_THAT (listener.lastArgs[0], WithinAbs (-45.0f, 0.5f));
    CHECK_THAT (listener.lastArgs[1], WithinAbs (30.0f, 0.5f));
    CHECK_THAT (listener.lastArgs[2], WithinAbs (0.6f, 0.01f));

    receiver.removeListener (&listener);
    sender.disconnect();
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: sendPosition ignores calls while disconnected", "[osc][send][edge]")
{
    juce::OSCReceiver receiver;
    CapturingListener listener;
    REQUIRE (receiver.connect (kTestSenderPort + 1));
    receiver.addListener (&listener);

    ADMOSCSender sender;
    CHECK_FALSE (sender.isConnected());

    // Not connected -- must be a silent no-op (matches pre-move OSD behavior
    // of gating all sends on oscSendConnected).
    sender.sendPosition (0, 10.0f, 10.0f, 1.0f);

    // Give any accidental send time to arrive before asserting it did not.
    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    CHECK (listener.messageCount.load() == 0);

    receiver.removeListener (&listener);
    receiver.disconnect();
}

TEST_CASE ("ADMOSCSender: sendPosition ignores out-of-range object index", "[osc][send][edge]")
{
    juce::OSCReceiver receiver;
    CapturingListener listener;
    REQUIRE (receiver.connect (kTestSenderPort + 2));
    receiver.addListener (&listener);

    ADMOSCSender sender;
    REQUIRE (sender.connect ("127.0.0.1", kTestSenderPort + 2));

    sender.sendPosition (-1, 10.0f, 10.0f, 1.0f);   // below range
    sender.sendPosition (MAX_SOURCES, 10.0f, 10.0f, 1.0f); // at/above range (0-based, so == out of range)

    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    CHECK (listener.messageCount.load() == 0);

    receiver.removeListener (&listener);
    sender.disconnect();
    receiver.disconnect();
}
