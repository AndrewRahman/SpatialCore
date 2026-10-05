// SpatialCoreUITests entry point (Phase 4, Plan 04-05).
//
// Catch2WithMain supplies main(). The JUCE GUI initialiser is created once per test run from an
// event listener, so every case can construct components, take Desktop::getMainMouseSource() and
// paint, and the JUCE GUI is torn down once at the end. Listing tests (build-time discovery by
// catch_discover_tests) returns before a run starts, so discovery never initialises the GUI.

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace
{

struct JuceGuiListener : Catch::EventListenerBase
{
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting (Catch::TestRunInfo const&) override
    {
        init = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    }

    void testRunEnded (Catch::TestRunStats const&) override
    {
        init.reset();
    }

    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> init;
};

} // namespace

CATCH_REGISTER_LISTENER (JuceGuiListener)
