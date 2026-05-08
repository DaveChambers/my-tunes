#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;

// Table view of the scanned tracks. Stub for the skeleton commit:
// just paints a placeholder. Real columns + selection + double-click
// land in the folder-scan commit.
class TrackListComponent : public juce::Component
{
public:
    explicit TrackListComponent (TrackLibrary& library);
    ~TrackListComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Called when the underlying library is repopulated (after a scan).
    void refresh();

private:
    TrackLibrary& library;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};
