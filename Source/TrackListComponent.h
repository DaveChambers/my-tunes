#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class TrackLibrary;
class TrackPreviewPlayer;

// Table view of the scanned tracks. Implements TableListBoxModel directly.
//
// The list works in terms of the visible-tracks index list MainComponent
// maintains; row N maps to library index visibleIndices[N] (or N if no
// filter).
//
// Listens to:
//   - TrackLibrary  (scan / tag changes -> repaint)
//   - TrackPreviewPlayer (play/stop -> repaint Transport column icons)
class TrackListComponent : public juce::Component,
                            private juce::TableListBoxModel,
                            private juce::ChangeListener
{
public:
    TrackListComponent (TrackLibrary& library, TrackPreviewPlayer& player);
    ~TrackListComponent() override;

    void resized() override;

    void refresh();
    void setVisibleTrackIndices (std::vector<int> indices);

    // Row clicked (left button). Argument is library index, or -1.
    std::function<void (int trackIndex)> onTrackSelected;

    // Transport cell clicked: caller should toggle playback for this track.
    std::function<void (int trackIndex)> onPlayToggleRequested;

private:
    enum ColumnIds
    {
        ColKey       = 1,
        ColBpm       = 2,
        ColArtist    = 3,
        ColTitle     = 4,
        ColVersion   = 5,
        ColTags      = 6,
        ColPriority  = 7,
        ColDiscogs   = 8,
        ColTransport = 9,
    };

    int  getNumRows() override;
    void paintRowBackground (juce::Graphics&, int rowNumber, int width, int height,
                             bool rowIsSelected) override;
    void paintCell (juce::Graphics&, int rowNumber, int columnId, int width, int height,
                    bool rowIsSelected) override;
    void cellClicked (int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showTagPopupMenu (int rowNumber);
    int  rowToLibraryIndex (int rowNumber) const;

    TrackLibrary&       library;
    TrackPreviewPlayer& player;
    juce::TableListBox  table;

    std::vector<int> visibleIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};
