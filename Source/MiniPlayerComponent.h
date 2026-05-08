#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;
class TrackPreviewPlayer;

// Strip above the footer with: track text | play/stop | scrubber | time.
//
// Updates from a 15 Hz juce::Timer (avoids a callback storm and is
// plenty for visible position movement). While the user is dragging
// the slider we stop pulling position from the player so the thumb
// doesn't fight the user; on drag-end we push the chosen position to
// the player.
class MiniPlayerComponent : public juce::Component,
                             private juce::Timer,
                             private juce::ChangeListener
{
public:
    MiniPlayerComponent (TrackLibrary& library, TrackPreviewPlayer& player);
    ~MiniPlayerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void onTransportButton();
    void refreshFromPlayer();
    void updateLayoutEnabled (bool playing);

    static juce::String formatTime (double seconds);

    TrackLibrary&       library;
    TrackPreviewPlayer& player;

    juce::Label      titleLabel;
    juce::Label      timeLabel;
    juce::TextButton transportButton { "Stop" };
    juce::Slider     positionSlider;

    bool userIsDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniPlayerComponent)
};
