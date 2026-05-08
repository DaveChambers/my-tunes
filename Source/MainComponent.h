#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "ArtworkCache.h"
#include "DiscogsClient.h"
#include "EmojiTagBoardComponent.h"
#include "LookupController.h"
#include "TrackDetailComponent.h"
#include "TrackGridComponent.h"
#include "TrackLibrary.h"
#include "TrackListComponent.h"

// Top-level window contents.
//
// Toolbar:
//   [Choose Folder]  folder/path  [Scan]  [Lookup Covers]  [List | Grid]  status
//
// Sidebar = emoji boards (filter), centre = list or grid, right = detail
// panel, footer = Discogs attribution.
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
    // NOTE: declaration order matters for clean shutdown. The library
    // owns track data the LookupController's worker writes to via
    // juce::MessageManager::callAsync. We declare the library first,
    // then the controller — so on destruction the controller goes first
    // (cancels its thread, joins) before the library is gone.
    TrackLibrary     library;
    DiscogsClient    discogsClient;
    ArtworkCache     artworkCache;
    LookupController lookupController { library, discogsClient, artworkCache };

    // --- toolbar -----------------------------------------------------------
    juce::TextButton chooseFolderButton  { "Choose Folder" };
    juce::TextButton scanButton          { "Scan" };
    juce::TextButton lookupCoversButton  { "Lookup Covers" };
    juce::TextButton viewListButton      { "List" };
    juce::TextButton viewGridButton      { "Grid" };
    juce::Label      folderLabel;
    juce::Label      statusLabel;
    juce::Label      attributionLabel;

    // --- main area ---------------------------------------------------------
    EmojiTagBoardComponent emojiBoard;
    TrackListComponent     listView   { library };
    TrackGridComponent     gridView   { library };
    TrackDetailComponent   detailView { library };

    // --- runtime state -----------------------------------------------------
    ViewMode         viewMode           = ViewMode::List;
    int              activeBoardIndex   = -1;
    std::vector<int> visibleIndices;
    int              selectedTrackIndex = -1;
    bool             isLookingUp        = false;
    int              lookupDone         = 0;
    int              lookupTotal        = 0;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // --- helpers -----------------------------------------------------------
    void onChooseFolderClicked();
    void runScan();
    void startLookup();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void setViewMode (ViewMode mode);
    void onBoardClicked (int boardIndex);
    void onTrackSelected (int trackIndex);

    void recomputeVisibleIndices();
    void recomputeBoardCounts();
    void updateFolderLabel();
    void updateStatusLabel();
    void updateButtonsEnabled();

    static juce::File getDefaultStartFolder();

    static constexpr int kSampleSize = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
