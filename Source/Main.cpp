#include <juce_gui_basics/juce_gui_basics.h>

#include "MainComponent.h"

class TuneListApp : public juce::JUCEApplication
{
public:
    TuneListApp() = default;

    const juce::String getApplicationName() override        { return "TuneList"; }
    const juce::String getApplicationVersion() override     { return juce::String (MYTUNES_VERSION_STRING); }
    bool moreThanOneInstanceAllowed() override              { return false; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : juce::DocumentWindow (name,
                                    juce::Desktop::getInstance()
                                        .getDefaultLookAndFeel()
                                        .findColour (juce::ResizableWindow::backgroundColourId),
                                    juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, false);
            setResizeLimits (800, 500, 4000, 3000);
            centreWithSize (1200, 800);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (TuneListApp)
