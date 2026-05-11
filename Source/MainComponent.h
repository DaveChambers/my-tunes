#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "ArtworkCache.h"
#include "DiscogsClient.h"
#include "EmojiTagBoardComponent.h"
#include "LookupController.h"
#include "MiniPlayerComponent.h"
#include "SpotifyClient.h"
#include "TrackDetailComponent.h"
#include "TrackFile.h"
#include "TrackGridComponent.h"
#include "TrackLibrary.h"
#include "TrackListComponent.h"
#include "TrackPreviewPlayer.h"
#include "TrackStorage.h"
#include "TuneListConfig.h"

// Build label shown in the footer right so Dave can immediately tell
// which app build he's running. Bump the suffix on every behaviour
// change.
#ifndef MYTUNES_BUILD_LABEL
 #define MYTUNES_BUILD_LABEL "v0.1 - stop/play resumes from position"
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
    // `config` is the single point of truth for credentials and must be
    // constructed before the clients that take its values.
    TuneListConfig     config;
    TrackLibrary       library;
    SpotifyClient      spotifyClient { config.getSpotifyClientId(),
                                       config.getSpotifyClientSecret() };
    DiscogsClient      discogsClient { config.getDiscogsUserToken() };
    ArtworkCache       artworkCache;
    LookupController   lookupController { library, spotifyClient, discogsClient, artworkCache };
    TrackPreviewPlayer previewPlayer;
    TrackStorage       storage;

    juce::TextButton chooseFolderButton  { "Choose Folder" };
    juce::TextButton scanButton          { "Scan" };
    juce::TextButton lookupCoversButton  { "Re-lookup" };
    juce::TextButton viewListButton      { "List" };
    juce::TextButton viewGridButton      { "Grid" };
    juce::Label      folderLabel;
    juce::Label      statusLabel;
    juce::Label      diagnosticsLabel;
    juce::Label      attributionLabel;
    juce::Label      versionLabel;

    EmojiTagBoardComponent emojiBoard;
    TrackListComponent     listView   { library, previewPlayer };
    TrackGridComponent     gridView   { library, previewPlayer };
    TrackDetailComponent   detailView { library, previewPlayer };
    MiniPlayerComponent    miniPlayer { library, previewPlayer };

    ViewMode         viewMode           = ViewMode::List;
    int              activeBoardIndex   = -1;
    std::vector<int> visibleIndices;
    int              selectedTrackIndex = -1;

    // Artwork lookup state, all maintained on the message thread.
    // Counters reflect the FINAL artwork source chosen per track —
    // Spotify-first with Discogs fallback — so they answer the question
    // "where did Dave's covers actually come from this batch".
    bool             isLookingUp       = false;
    int              lookupDone        = 0;
    int              lookupTotal       = 0;
    int              countSpotifyArt   = 0;   // artworkSource == Spotify
    int              countDiscogsArt   = 0;   // artworkSource == Discogs
    int              countNoArtwork    = 0;   // artworkSource == None
    int              countSpotifyError = 0;
    int              countDiscogsError = 0;
    juce::String     lastSpotifyError;
    juce::String     lastDiscogsError;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void onChooseFolderClicked();
    void runScan();
    void startAutoLookup();          // kicks the controller; auto after scan
    void runManualLookup();          // toolbar "Re-lookup" — same path

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

    static constexpr int kSampleSize = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
