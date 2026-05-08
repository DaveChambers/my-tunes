#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TrackLibrary;

// Table view of the scanned tracks. Implements the JUCE
// TableListBoxModel directly. Columns:
//   Key | BPM | Artist | Title | Version | Tags | Priority | Discogs
class TrackListComponent : public juce::Component,
                            private juce::TableListBoxModel
{
public:
    explicit TrackListComponent (TrackLibrary& library);
    ~TrackListComponent() override;

    void resized() override;

    // Repopulate from the underlying library (call after a folder scan).
    void refresh();

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

    TrackLibrary&     library;
    juce::TableListBox table;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};
