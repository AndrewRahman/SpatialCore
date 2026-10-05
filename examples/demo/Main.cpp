// SpatialCoreDemo entry point (Phase 4, D-01 to D-03).
//
// Modes:
//   SpatialCoreDemo --screenshots <dir>   no window, no audio device: writes before-drag.png and
//                                         after-drag.png, prints the report, exits 0 or 1
//   SpatialCoreDemo                       interactive: a window with the map

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

        component = std::make_unique<DemoComponent>();
        component->prepareRender (48000.0, 512);
        window = std::make_unique<MainWindow> (getApplicationName(), *component, *this);
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
