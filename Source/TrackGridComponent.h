#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class TrackLibrary;
class TrackPreviewPlayer;

// Tile/card grid of tracks. Cards show:
//   - cover area (cached thumbnail or placeholder)
//   - centred play / stop overlay (click toggles preview playback)
//   - artist, title, key + BPM, Discogs status, emoji tags
class TrackGridComponent : public juce::Component,
                            private juce::ChangeListener
{
public:
    TrackGridComponent (TrackLibrary& library, TrackPreviewPlayer& player);
    ~TrackGridComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    // Find which card index sits under the local-space point `p`, or
    // -1 if none. Exposed so sister components / unit tests can ask
    // about hit-testing without reimplementing the layout maths.
    int  cardIndexAt (juce::Point<int> p) const;

    void refresh();
    void setVisibleTrackIndices (std::vector<int> indices);

    std::function<void (int trackIndex)> onTrackSelected;
    std::function<void (int trackIndex)> onPlayToggleRequested;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    int  numVisible() const;
    int  visibleAt (int n) const;
    juce::Rectangle<int> cardRect    (int cardIndex, int cardW, int cardH, int cols) const;
    juce::Rectangle<int> playHitRect (juce::Rectangle<int> coverArea) const;

    void paintCard (juce::Graphics& g, int libraryIndex,
                    juce::Rectangle<int> bounds, bool selected, bool isPlayingThis) const;
    void showTagPopupMenu (int trackIndex);

    TrackLibrary&       library;
    TrackPreviewPlayer& player;
    std::vector<int>    visibleIndices;
    int                 selectedCardIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackGridComponent)
};
