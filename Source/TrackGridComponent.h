#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

class TrackLibrary;
class TrackPreviewPlayer;

// Tile/card grid of tracks. Cards show:
//   - cover area (cached thumbnail or placeholder)
//   - centred play / stop overlay (click toggles preview playback)
//   - artist, title, key + BPM, Discogs/Spotify status, emoji tags
//
// The grid lives inside a juce::Viewport so any number of rows can be
// scrolled — without a viewport, the bottom rows fall behind the
// mini-player / footer strip and become unclickable. The inner
// CardsView holds the painting + mouse logic; this outer class is a
// thin host that wires the viewport, owns the visible-index state,
// and exposes the public API to MainComponent.
class TrackGridComponent : public juce::Component,
                            private juce::ChangeListener
{
public:
    TrackGridComponent (TrackLibrary& library, TrackPreviewPlayer& player);
    ~TrackGridComponent() override;

    void resized() override;

    void refresh();
    void setVisibleTrackIndices (std::vector<int> indices);

    std::function<void (int trackIndex)> onTrackSelected;
    std::function<void (int trackIndex)> onPlayToggleRequested;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    class CardsView;

    int  numVisible() const;
    int  visibleAt  (int n) const;

    TrackLibrary&                library;
    TrackPreviewPlayer&          player;
    std::vector<int>             visibleIndices;

    juce::Viewport               viewport;
    std::unique_ptr<CardsView>   cards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackGridComponent)
};
