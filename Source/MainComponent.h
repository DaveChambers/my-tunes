#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "EmojiTagBoardComponent.h"
#include "TrackDetailComponent.h"
#include "TrackGridComponent.h"
#include "TrackLibrary.h"
#include "TrackListComponent.h"

// Top-level window contents.
//
// Layout:
//   ┌──────────────────── toolbar ─────────────────────┐
//   │ [Choose Folder] folder/path  [Scan]  [List|Grid]  │
//   │                                            status │
//   ├─────────┬──────────────────────────────┬────────┤
//   │ emoji   │  list view OR grid view       │ detail │
//   │ boards  │                               │        │
//   ├─────────┴───────────────────────────────┴────────┤
//   │              Discogs attribution                  │
//   └────────────────────────────────────────────────────┘
//
// View toggle, filter from emoji boards, right-click tagging in list,
// detail panel — all live here. Discogs network lookup is still
// stubbed.
class MainComponent : public juce::Component,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class ViewMode { List, Grid };

    // --- model -------------------------------------------------------------
    TrackLibrary library;

    // --- toolbar -----------------------------------------------------------
    juce::TextButton chooseFolderButton { "Choose Folder" };
    juce::TextButton scanButton         { "Scan" };
    juce::TextButton viewListButton     { "List" };
    juce::TextButton viewGridButton     { "Grid" };
    juce::Label      folderLabel;
    juce::Label      statusLabel;
    juce::Label      attributionLabel;

    // --- main area ---------------------------------------------------------
    EmojiTagBoardComponent emojiBoard;
    TrackListComponent     listView   { library };
    TrackGridComponent     gridView   { library };
    TrackDetailComponent   detailView { library };

    // --- runtime state -----------------------------------------------------
    ViewMode         viewMode         = ViewMode::List;
    int              activeBoardIndex = -1;     // -1 = no filter
    std::vector<int> visibleIndices;            // computed from filter
    int              selectedTrackIndex = -1;   // library index, -1 = none

    std::unique_ptr<juce::FileChooser> fileChooser;

    // --- helpers -----------------------------------------------------------
    void onChooseFolderClicked();
    void runScan();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void setViewMode (ViewMode mode);
    void onBoardClicked (int boardIndex);
    void onTrackSelected (int trackIndex);

    void recomputeVisibleIndices();
    void recomputeBoardCounts();
    void updateFolderLabel();
    void updateStatusLabel();

    static juce::File getDefaultStartFolder();

    static constexpr int kSampleSize = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
