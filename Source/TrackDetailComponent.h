#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;

// Details for the currently selected track (filename, parsed fields,
// emoji tags, priority slider, notes, Discogs match, cover image).
// Stub for the skeleton commit.
class TrackDetailComponent : public juce::Component
{
public:
    explicit TrackDetailComponent (TrackLibrary& library);
    ~TrackDetailComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Index into the library's track vector; -1 means "no selection".
    void setSelectedTrackIndex (int index);

private:
    TrackLibrary& library;
    int           selectedIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDetailComponent)
};
