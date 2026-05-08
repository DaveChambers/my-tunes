#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;

// Tile/card grid of tracks (cover, artist, title, BPM/key, emoji tags).
// Stub for the skeleton commit.
class TrackGridComponent : public juce::Component
{
public:
    explicit TrackGridComponent (TrackLibrary& library);
    ~TrackGridComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void refresh();

private:
    TrackLibrary& library;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackGridComponent)
};
