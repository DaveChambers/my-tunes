#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class TrackLibrary;

// Table view of the scanned tracks. Implements TableListBoxModel directly.
//
// Contract with MainComponent:
//   - The list works in terms of the *visible-tracks index list* the
//     MainComponent maintains. Each visible-row N maps to the library
//     index visibleIndices[N]. With no filter this is just 0..N-1.
//   - Library changes (scan, tag toggle) reach the list via
//     juce::ChangeListener — the library is a juce::ChangeBroadcaster.
class TrackListComponent : public juce::Component,
                            private juce::TableListBoxModel,
                            private juce::ChangeListener
{
public:
    explicit TrackListComponent (TrackLibrary& library);
    ~TrackListComponent() override;

    void resized() override;

    // Repopulate from the underlying library. Idempotent.
    void refresh();

    // Set which library indices the list should display, in order.
    // Pass an empty vector to fall back to "all tracks" (full library).
    void setVisibleTrackIndices (std::vector<int> indices);

    // Notification: row clicked (left button). Argument is the *library
    // index* of the clicked track, or -1 if no row is selected.
    std::function<void (int trackIndex)> onTrackSelected;

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
    };

    // TableListBoxModel ----------------------------------------------------
    int  getNumRows() override;
    void paintRowBackground (juce::Graphics&, int rowNumber, int width, int height,
                             bool rowIsSelected) override;
    void paintCell (juce::Graphics&, int rowNumber, int columnId, int width, int height,
                    bool rowIsSelected) override;
    void cellClicked (int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void selectedRowsChanged (int lastRowSelected) override;

    // ChangeListener -------------------------------------------------------
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showTagPopupMenu (int rowNumber);
    int  rowToLibraryIndex (int rowNumber) const;

    TrackLibrary&      library;
    juce::TableListBox table;

    // Empty == "show all". Otherwise exact indices to show, in order.
    std::vector<int> visibleIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};
