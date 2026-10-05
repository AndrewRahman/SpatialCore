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

// ----------------------------------------------------------------------------
// Every position address moves the sound (D-11). Messages go through
// testProcessOSCMessage, so the real-UDP hop is not repeated; each case checks a rendered
// channel level, and the single-axis addresses also check the NaN merge kept the other axes.
// ----------------------------------------------------------------------------

TEST_CASE ("OSC route: /azim moves the rendered image and the merge keeps elevation and distance",
           "[route][osc]")
{
    SyncRoute route (OutputFormat::Binaural, 2);
    route.rig.setObject (0, 0.0f, 0.0f, 0.5f);

    route.send ("/adm/obj/1/azim", { 90.0f });
    const float ratioLeft = route.rig.leftRightRatio();
    CAPTURE (ratioLeft);
    CHECK (ratioLeft >= 2.0f);
    CHECK (route.rig.object (0).elevationDeg == 0.0f);
    CHECK (route.rig.object (0).distance == 0.5f);

    route.send ("/adm/obj/1/azim", { -90.0f });
    const float ratioRight = route.rig.leftRightRatio();
    CAPTURE (ratioRight);
    CHECK (ratioRight <= 0.5f);
    CHECK (route.rig.object (0).elevationDeg == 0.0f);
    CHECK (route.rig.object (0).distance == 0.5f);
    CHECK (route.glue.positionsReceived == 2);
}

TEST_CASE ("OSC route: /elev pulls a left image toward the centre and keeps azimuth",
           "[route][osc]")
{
    SyncRoute route (OutputFormat::Binaural, 2);
    route.rig.setObject (0, 90.0f, 0.0f, 0.5f);

    const float before = route.rig.leftRightRatio();
    CAPTURE (before);
    CHECK (before >= 2.0f);

    route.send ("/adm/obj/1/elev", { 75.0f });
    const float after = route.rig.leftRightRatio();
    CAPTURE (after);
    CHECK (after < 1.6f);
    CHECK (after > 1.0f);
    CHECK (route.rig.object (0).azimuthDeg == 90.0f);
    CHECK (route.rig.object (0).distance == 0.5f);
}

TEST_CASE ("OSC route: /dist makes a far object quieter and keeps azimuth",
           "[route][osc]")
{
    SyncRoute route (OutputFormat::Binaural, 2);
    route.rig.setObject (0, 90.0f, 0.0f, 0.2f);

    const float nearLeft = route.rig.renderRms()[0];
    CAPTURE (nearLeft);
    REQUIRE (nearLeft > 0.0f);

    route.send ("/adm/obj/1/dist", { 0.9f });
    const float farLeft = route.rig.renderRms()[0];
    CAPTURE (farLeft);
    CHECK (farLeft < 0.5f * nearLeft);
    CHECK (route.rig.object (0).azimuthDeg == 90.0f);
    CHECK (route.rig.object (0).elevationDeg == 0.0f);
}

TEST_CASE ("OSC route: /aed moves the image on Binaural and picks the Quad speaker",
           "[route][osc]")
{
    SECTION ("Binaural left then right")
    {
        SyncRoute route (OutputFormat::Binaural, 2);
        route.rig.setObject (0, 0.0f, 0.0f, 0.5f);

        route.send ("/adm/obj/1/aed", { 90.0f, 0.0f, 0.5f });
        const float ratioLeft = route.rig.leftRightRatio();
        CAPTURE (ratioLeft);
        CHECK (ratioLeft >= 2.0f);

        route.send ("/adm/obj/1/aed", { -90.0f, 0.0f, 0.5f });
        const float ratioRight = route.rig.leftRightRatio();
        CAPTURE (ratioRight);
        CHECK (ratioRight <= 0.5f);
    }

    SECTION ("Quad: +45 is channel 0, -45 is channel 1, the rest stay silent")
    {
        SyncRoute route (OutputFormat::Quad, 4);
        route.rig.setObject (0, 0.0f, 0.0f, 0.5f);

        for (const auto& [az, expected] : { std::pair<float, int> { 45.0f, 0 },
                                            std::pair<float, int> { -45.0f, 1 } })
        {
            route.send ("/adm/obj/1/aed", { az, 0.0f, 0.5f });
            const auto rms = route.rig.renderRms();
            const int loudest = RouteRenderRig::loudestChannel (rms, 4);
            CAPTURE (az, loudest, rms[0], rms[1], rms[2], rms[3]);
            CHECK (loudest == expected);
            for (int c = 0; c < 4; ++c)
                if (c != loudest)
                    CHECK (rms[(size_t) c] < 0.01f * rms[(size_t) loudest]);
        }
    }
}

TEST_CASE ("OSC route: /xyz moves the image (ADM x = -0.5 is left)", "[route][osc]")
{
    SyncRoute route (OutputFormat::Binaural, 2);
    route.rig.setObject (0, 0.0f, 0.0f, 0.5f);

    route.send ("/adm/obj/1/xyz", { -0.5f, 0.0f, 0.0f });
    const float ratioLeft = route.rig.leftRightRatio();
    CAPTURE (ratioLeft);
    CHECK (ratioLeft >= 2.0f);

    route.send ("/adm/obj/1/xyz", { 0.5f, 0.0f, 0.0f });
    const float ratioRight = route.rig.leftRightRatio();
    CAPTURE (ratioRight);
    CHECK (ratioRight <= 0.5f);
}
