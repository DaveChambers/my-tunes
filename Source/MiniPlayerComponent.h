#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;
class TrackPreviewPlayer;

// Bottom playback strip:
//
//   ┌──────────────────────────────────────────────┐
//   │ [-----------scrub bar (full width)---------] │
//   │ 01:24                  ▶                04:23 │
//   └──────────────────────────────────────────────┘
//
// Below this in MainComponent sits the Discogs attribution footer.
//
// 15 Hz Timer pulls position from the player. While the user drags
// the slider we don't pull from the player so the thumb doesn't
// fight the user; on drag-end we push the chosen position back.
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
    // A small chunky-geometric button that paints either a triangle
    // (play) or a square (stop) depending on its state. No text.
    class TransportIconButton : public juce::Button
    {
    public:
        TransportIconButton();
        void setShowingStop (bool stopState);
    private:
        void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
        bool showingStop = false;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void onTransportButton();
    void refreshFromPlayer();
    static juce::String formatTime (double seconds);

    TrackLibrary&       library;
    TrackPreviewPlayer& player;

    juce::Label         elapsedLabel;
    juce::Label         remainingLabel;
    TransportIconButton transportButton;
    juce::Slider        positionSlider;

    bool userIsDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniPlayerComponent)
};
