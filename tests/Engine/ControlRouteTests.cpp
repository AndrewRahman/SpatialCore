#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <juce_osc/juce_osc.h>
#include "../Support/RouteRenderRig.h"
#include <cmath>

using namespace spatialcore;

// ============================================================================
// Control routes (Phase 4, EXTR-04, D-11 / D-12 / D-18).
//
// A control route is proven by the SOUND moving, never by a position value arriving: each test
// sends ADM-OSC, lets consumer-style glue copy the position into the rig, renders through
// RenderEngine with engineComputesGains and engineDerivesDispatch set, and asserts the channel
// levels. The glue below is what a plugin writes; no library file connects ADMOSCReceiver to
// RenderEngine (D-12).
// ============================================================================

namespace
{
    using spatialcore::test::RouteRenderRig;

    // The consumer's ADMOSCReceiver::Listener: NaN-aware merge into the rig's object state.
    struct OscToRigGlue : ADMOSCReceiver::Listener
    {
        explicit OscToRigGlue (RouteRenderRig& r) : rig (r) {}

        void admPositionReceived (int objectIndex, float azimuthDeg,
                                  float elevationDeg, float distance) override
        {
            rig.mergeObject (objectIndex, azimuthDeg, elevationDeg, distance);
            ++positionsReceived;
        }

        RouteRenderRig& rig;
        int positionsReceived = 0;
    };

    // Delivers MessageLoopCallback messages: pumps the message loop in 50 ms slices until
    // `counter` reaches `expected` or two seconds have passed.
    void pumpUntil (const int& counter, int expected)
    {
        for (int i = 0; i < 40 && counter < expected; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    juce::OSCMessage floats (const char* address, std::initializer_list<float> values)
    {
        juce::OSCMessage m { juce::OSCAddressPattern { juce::String (address) } };
        for (float v : values)
            m.addFloat32 (v);
        return m;
    }

    // Synchronous send, no socket: the real-UDP hop is proven once by the tracer.
    struct SyncRoute
    {
        SyncRoute (OutputFormat format, int numOutChannels)
            : rig (format, numOutChannels), glue (rig)
        {
            rx.addListener (&glue);
        }
        ~SyncRoute() { rx.removeListener (&glue); }

        void send (const char* address, std::initializer_list<float> values)
        {
            rx.testProcessOSCMessage (floats (address, values));
        }

        RouteRenderRig rig;
        OscToRigGlue glue;
        ADMOSCReceiver rx;
    };
}

TEST_CASE ("OSC route: an external ADM-OSC sender over UDP moves a rendered object left then right",
           "[route][osc][udp][tracer]")
{
    // Without a JUCE GUI initialiser the MessageLoopCallback delivery segfaults in this
    // GUI-less executable, so it comes first and goes out of scope last.
    juce::ScopedJuceInitialiser_GUI juceInit;

    RouteRenderRig rig (OutputFormat::Binaural, 2);
    rig.setObject (0, 0.0f, 0.0f, 0.5f);

    OscToRigGlue glue (rig);
    ADMOSCReceiver rx;
    rx.addListener (&glue);
    REQUIRE (rx.connect (9720));

    // The external device: a plain OSC sender, not a SpatialCore class.
    juce::OSCSender desk;
    REQUIRE (desk.connect ("127.0.0.1", 9720));

    REQUIRE (desk.send (floats ("/adm/obj/1/aed", { 90.0f, 0.0f, 0.5f })));
    pumpUntil (glue.positionsReceived, 1);
    REQUIRE (glue.positionsReceived == 1);
    const float ratioLeft = rig.leftRightRatio();
    CAPTURE (ratioLeft);
    CHECK (ratioLeft >= 2.0f);

    REQUIRE (desk.send (floats ("/adm/obj/1/aed", { -90.0f, 0.0f, 0.5f })));
    pumpUntil (glue.positionsReceived, 2);
    REQUIRE (glue.positionsReceived == 2);
    const float ratioRight = rig.leftRightRatio();
    CAPTURE (ratioRight);
    CHECK (ratioRight <= 0.5f);

    desk.disconnect();
    rx.disconnect();
    rx.removeListener (&glue);
}
