#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "ArtworkCache.h"
#include "EmojiTagBoardComponent.h"
#include "LookupController.h"
#include "MetadataProxyClient.h"
#include "MiniPlayerComponent.h"
#include "TrackDetailComponent.h"
#include "TrackFile.h"
#include "TrackGridComponent.h"
#include "TrackLibrary.h"
#include "TrackListComponent.h"
#include "TrackPreviewPlayer.h"
#include "TrackStorage.h"

// Build label shown in the footer right so Dave can immediately tell
// which app build he's running. Bump the suffix on every behaviour
// change.
#ifndef MYTUNES_BUILD_LABEL
 #define MYTUNES_BUILD_LABEL "v0.1 - artwork cache actually respected"
#endif

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

    // Member ordering matters for clean shutdown:
    // controller is destroyed before library + the helpers it depends on.
    TrackLibrary        library;
    MetadataProxyClient metadataClient;
    ArtworkCache        artworkCache;
    LookupController    lookupController { library, metadataClient, artworkCache };
    TrackPreviewPlayer  previewPlayer;
    TrackStorage        storage;

    juce::TextButton chooseFolderButton  { "Choose Folder" };
    juce::TextButton scanButton          { "Scan" };
    juce::TextButton lookupCoversButton  { "Re-lookup" };
    juce::TextButton cancelButton        { "Cancel" };
    juce::TextButton viewListButton      { "List" };
    juce::TextButton viewGridButton      { "Grid" };
    juce::Label      folderLabel;
    juce::Label      statusLabel;
    juce::Label      diagnosticsLabel;
    juce::Label      attributionLabel;
    juce::Label      versionLabel;

    juce::TextEditor searchField;
    juce::TextButton searchClearButton  { "x" };

    EmojiTagBoardComponent emojiBoard;
    TrackListComponent     listView   { library, previewPlayer };
    TrackGridComponent     gridView   { library, previewPlayer };
    TrackDetailComponent   detailView { library, previewPlayer };
    MiniPlayerComponent    miniPlayer { library, previewPlayer };

    ViewMode         viewMode           = ViewMode::List;
    int              activeBoardIndex   = -1;
    std::vector<int> visibleIndices;
    int              selectedTrackIndex = -1;

    // Search state. The field's TextEditor::onTextChange scribbles
    // into `searchText` (lowercased) and (re)starts the debounce
    // timer. The timer's callback runs recomputeVisibleIndices,
    // which ANDs `searchText` with `activeBoardIndex`.
    juce::String     searchText;

    // Artwork lookup state, all maintained on the message thread.
    bool             isLookingUp     = false;
    int              lookupDone      = 0;
    int              lookupTotal     = 0;
    int              countSpotifyArt = 0;
    int              countDiscogsArt = 0;
    int              countNoArtwork  = 0;
    int              countRejected   = 0;
    int              countLookupError = 0;
    juce::String     lastLookupError;

    // How many tracks in the current library already have valid
    // cached artwork at the moment auto/manual lookup last started.
    // Lookup never touches these tracks (Re-lookup bypasses the
    // cache check, so manual force-refresh still works).
    int              countCachedAtLookupStart = 0;

    // Async scan state. The scanner walks the chosen folder on a
    // worker thread (juce::Thread) and posts batches of parsed
    // TrackFile objects back via MessageManager::callAsync. Counts
    // and the live "scanning..." label are message-thread state.
    bool             isScanning   = false;
    int              scanFound    = 0;
    juce::File       scanFolder;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // Background folder scanner. Walks the directory iterator on a
    // worker thread and batches parsed TrackFile objects back to the
    // message thread. Lives here so its lifetime is bounded by
    // MainComponent's: the destructor stops it before any of the
    // members it touches go away.
    class FolderScanner;
    std::unique_ptr<FolderScanner> folderScanner;

    void onChooseFolderClicked();
    void runScan();
    void startFolderScan (juce::File folder);
    void onScanBatchReady (std::vector<TrackFile> batch, int totalFound, bool done);
    void onScanCancelled();
    void cancelInFlightWork();
    void startAutoLookup();          // kicks the controller; auto after scan
    void runManualLookup();          // toolbar "Re-lookup" — same path
    void onCancelButtonClicked();

    // Save coalescer. Per-library broadcast (re)starts a short timer;
    // when it fires, TrackStorage::snapshotAndSave runs once. Without
    // this the 8 000-track JSON gets rewritten on every per-track
    // lookup broadcast.
    class SaveDebouncer;
    std::unique_ptr<SaveDebouncer> saveDebouncer;
    void scheduleSave();
    void flushSaveNow();

    // Search field debounce. Every keystroke (re)schedules this;
    // when it fires we run recomputeVisibleIndices once. Keeps
    // 8 000-track filtering responsive without re-filtering on
    // every keystroke.
    class SearchDebouncer;
    std::unique_ptr<SearchDebouncer> searchDebouncer;
    void onSearchFieldChanged();
    void onSearchClearClicked();
    void applySearchNow();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void setViewMode (ViewMode mode);
    void onBoardClicked (int boardIndex);
    void onTrackSelected (int trackIndex);

    void recomputeVisibleIndices();
    void recomputeBoardCounts();
    void resetLookupCounters();
    void onTrackLookupProcessed (int trackIndex,
                                  const LookupController::TrackOutcome&);

    void togglePlayback (int trackIndex);

    void updateFolderLabel();
    void updateStatusLabel();
    void updateDiagnosticsLabel();
    void updateButtonsEnabled();

    static juce::File getDefaultStartFolder();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
