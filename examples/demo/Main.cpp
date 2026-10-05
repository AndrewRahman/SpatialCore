// SpatialCoreDemo entry point (Phase 4, D-01 to D-03).
//
// Modes:
//   SpatialCoreDemo --screenshots <dir>   no window, no audio device: writes before-drag.png and
//                                         after-drag.png, prints the report, exits 0 or 1
//   SpatialCoreDemo --selftest            no window, no audio device: drives the OSC, query,
//                                         trajectory and map routes through RenderEngine on
//                                         loopback ports 9790 and 9791, exits 0 on PASS, else 1
//   SpatialCoreDemo [--osc-in <port>] [--osc-out <port>]
//                                         interactive: a window with the map, the default audio
//                                         device, ADM-OSC in on 4002 and out to 127.0.0.1:4003

#include "DemoComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <memory>

class SpatialCoreDemoApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();

        const int shots = args.indexOf ("--screenshots");
        if (shots >= 0)
        {
            runScreenshots (args, shots);
            return;
        }

        if (args.contains ("--selftest"))
        {
            runSelfTest();
            return;
        }

        runInteractive (args);
    }

    void shutdown() override
    {
        window.reset();
        component.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, DemoComponent& content, juce::JUCEApplication& owner)
            : juce::DocumentWindow (name, juce::Colours::black, juce::DocumentWindow::allButtons),
              app (owner)
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&content, true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        juce::JUCEApplication& app;
    };

    // The return value is set before quit() so the process exits with it.
    void finish (bool ok)
    {
        setApplicationReturnValue (ok ? 0 : 1);
        quit();
    }

    static int portOption (const juce::StringArray& args, const juce::String& flag, int fallback)
    {
        const int at = args.indexOf (flag);
        return (at >= 0 && at + 1 < args.size()) ? args[at + 1].getIntValue() : fallback;
    }

    void runInteractive (const juce::StringArray& args)
    {
        component = std::make_unique<DemoComponent>();

        // Opening the audio device calls prepareRender() with the device's rate and buffer size.
        const auto audioError = component->startAudio();
        if (audioError.isNotEmpty())
            std::cerr << "audio device: " << audioError.toStdString() << "\n";

        const int inPort = portOption (args, "--osc-in", 4002);
        const int outPort = portOption (args, "--osc-out", 4003);
        if (component->startOsc (inPort, outPort))
            std::cout << "ADM-OSC in on " << inPort << ", out to 127.0.0.1:" << outPort << "\n";
        else
            std::cerr << "ADM-OSC not started: ports " << inPort << " and " << outPort
                      << " conflict or cannot be opened\n";

        component->startUpdates();
        window = std::make_unique<MainWindow> (getApplicationName(), *component, *this);
    }

    void runSelfTest()
    {
        component = std::make_unique<DemoComponent>();
        component->prepareRender (48000.0, 512);
        component->runSelfTest ([this] (bool ok) { finish (ok); });
    }

    void runScreenshots (const juce::StringArray& args, int flagIndex)
    {
        if (flagIndex + 1 >= args.size())
        {
            std::cerr << "--screenshots needs a folder\n";
            finish (false);
            return;
        }

        const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (args[flagIndex + 1]);

        component = std::make_unique<DemoComponent>();
        component->prepareRender (48000.0, 512);

        juce::String report;
        const bool ok = component->writeDragScreenshots (dir, report);

        std::cout << report.toStdString();
        if (! ok)
            std::cerr << "could not write the screenshots into " << dir.getFullPathName().toStdString() << "\n";

        finish (ok);
    }

    std::unique_ptr<DemoComponent> component;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (SpatialCoreDemoApplication)
