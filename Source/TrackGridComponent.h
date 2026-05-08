#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class TrackLibrary;

// Tile/card grid of tracks. Cards show:
//   - cover area (placeholder until DiscogsClient + ArtworkCache wire up)
//   - artist, title
//   - key + BPM
//   - emoji tags
//   - Discogs status
//
// Same visible-indices contract as TrackListComponent.
class TrackGridComponent : public juce::Component,
                            private juce::ChangeListener
{
public:
    explicit TrackGridComponent (TrackLibrary& library);
    ~TrackGridComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    void refresh();
    void setVisibleTrackIndices (std::vector<int> indices);

    std::function<void (int trackIndex)> onTrackSelected;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    int  numVisible() const;
    int  visibleAt (int n) const; // -> library index, or -1
    juce::Rectangle<int> cardRect (int cardIndex,
                                    int cardW, int cardH,
                                    int cols) const;

    void paintCard (juce::Graphics& g, int libraryIndex,
                    juce::Rectangle<int> bounds, bool selected) const;

    TrackLibrary&    library;
    std::vector<int> visibleIndices;
    int              selectedCardIndex = -1; // visible-card index

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackGridComponent)
};
