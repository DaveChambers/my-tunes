#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include "MainComponent.h"
#include "BinaryData.h"

//==============================================================================
// UI scale. macOS/Windows hand JUCE the desktop's scaling; on Linux JUCE often
// gets 1.0 even on a 3K laptop panel, so everything renders tiny. We pick a
// sensible starting scale from the screen's DPI and let the user zoom with
// Ctrl/Cmd + "=", "-", "0". The choice is remembered.
namespace UiScale
{
    static juce::PropertiesFile& settings()
    {
        static juce::PropertiesFile file ([] {
            juce::PropertiesFile::Options o;
            o.applicationName     = "TuneList";
            o.filenameSuffix      = ".settings";
            o.folderName          = "TuneList";
            o.osxLibrarySubFolder = "Application Support";
            return o;
        }());
        return file;
    }

    static float roundQuarter (double v)  { return (float) juce::jlimit (0.75, 3.0, std::round (v * 4.0) / 4.0); }

    // Scale to use when the user hasn't chosen one.
    static float automatic()
    {
        auto env = juce::SystemStats::getEnvironmentVariable ("TUNELIST_SCALE", {});
        if (env.getDoubleValue() > 0.0)
            return roundQuarter (env.getDoubleValue());

       #if JUCE_LINUX || JUCE_BSD
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            if (d->scale > 1.01)                      // the desktop already scales us
                return 1.0f;
            if (d->dpi >= 140.0)                       // physical DPI from the monitor
                return roundQuarter (d->dpi / 96.0);
            const auto w = d->logicalBounds.getWidth() * d->scale;   // fall back to pixel width
            if (w >= 3000) return 2.0f;
            if (w >= 2400) return 1.5f;
        }
       #endif
        return 1.0f;
    }

    static float current()
    {
        auto saved = (float) settings().getDoubleValue ("uiScale", 0.0);
        return saved > 0.0f ? saved : automatic();
    }

    static void apply (float scale, bool remember)
    {
        scale = roundQuarter (scale);
        juce::Desktop::getInstance().setGlobalScaleFactor (scale);
        if (remember)
        {
            settings().setValue ("uiScale", (double) scale);
            settings().saveIfNeeded();
        }
    }
}

class TuneListApp : public juce::JUCEApplication
{
public:
    TuneListApp() = default;

    const juce::String getApplicationName() override        { return "TuneList"; }
    const juce::String getApplicationVersion() override     { return juce::String (MYTUNES_VERSION_STRING); }
    bool moreThanOneInstanceAllowed() override              { return false; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        UiScale::apply (UiScale::current(), false);
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

    class MainWindow : public juce::DocumentWindow,
                       private juce::KeyListener
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

            // Window / taskbar icon. macOS takes it from the .app bundle; on
            // Linux an icon-less window shows the desktop's generic glyph.
            auto icon = juce::ImageCache::getFromMemory (BinaryData::TuneListIcon_png,
                                                         BinaryData::TuneListIcon_pngSize);
            if (icon.isValid())
                setIcon (icon.rescaled (256, 256, juce::Graphics::highResamplingQuality));

            setContentOwned (new MainComponent(), true);
            addKeyListener (this);
            setResizable (true, false);
            setResizeLimits (800, 500, 4000, 3000);
            centreWithSize (1200, 800);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        using juce::DocumentWindow::keyPressed;   // keep Component::keyPressed visible

        // Ctrl (Cmd on macOS) + "=" / "+" bigger, "-" smaller, "0" back to automatic.
        bool keyPressed (const juce::KeyPress& key, juce::Component*) override
        {
            if (! key.getModifiers().isCommandDown())
                return false;

            const auto c = key.getTextCharacter();
            const auto code = key.getKeyCode();
            const float now = juce::Desktop::getInstance().getGlobalScaleFactor();

            if (c == '=' || c == '+' || code == '=' || code == '+')
                UiScale::apply (now + 0.25f, true);
            else if (c == '-' || code == '-')
                UiScale::apply (now - 0.25f, true);
            else if (c == '0' || code == '0')
            {
                UiScale::settings().removeValue ("uiScale");
                UiScale::settings().saveIfNeeded();
                UiScale::apply (UiScale::automatic(), false);
            }
            else
                return false;

            return true;
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (TuneListApp)
