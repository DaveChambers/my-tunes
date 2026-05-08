#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;

// Right-side panel: details for the currently selected track.
//
// First version paints text directly; no inline editing yet beyond
// emoji-tag toggling via the right-click menu in the list view.
//
// Listens to the library so tag changes refresh immediately.
class TrackDetailComponent : public juce::Component,
                              private juce::ChangeListener
{
public:
    explicit TrackDetailComponent (TrackLibrary& library);
    ~TrackDetailComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Library index, or -1 for "no selection".
    void setSelectedTrackIndex (int index);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    TrackLibrary& library;
    int           selectedIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDetailComponent)
};
