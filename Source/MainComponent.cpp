#include "MainComponent.h"

#include "TrackFilenameParser.h"

// =============================================================================
// FolderScanner: a juce::Thread that walks a directory recursively, parses
// filenames, and posts batches of parsed TrackFile objects back to the
// message thread. The owning MainComponent stops it cleanly in its dtor.
// =============================================================================
class MainComponent::FolderScanner : public juce::Thread
{
public:
    FolderScanner (MainComponent& o)
        : juce::Thread ("TuneList-Scan"), owner (o) {}

    ~FolderScanner() override
    {
        stopThread (4000);
    }

    void scan (juce::File folder)
    {
        // Cancel anything in flight before starting a new scan.
        stopThread (4000);
        targetFolder = std::move (folder);
        startThread();
    }

private:
    void run() override
    {
        static constexpr std::size_t kBatchSize = 200;

        const auto& exts = TrackFilenameParser::supportedExtensions();
        std::vector<TrackFile> batch;
        batch.reserve (kBatchSize);
        int found = 0;

        for (const auto& entry : juce::RangedDirectoryIterator (
                                     targetFolder,
                                     /*recursive*/  true,
                                     "*",
                                     juce::File::findFiles))
        {
            if (threadShouldExit())
                return;

            const auto file = entry.getFile();
            const auto ext  = file.getFileExtension().substring (1).toLowerCase();
            if (! exts.contains (ext))
                continue;

            TrackFile tf;
            TrackFilenameParser::parse (tf, file);
            batch.push_back (std::move (tf));
            ++found;

            if (batch.size() >= kBatchSize)
            {
                postBatch (std::move (batch), found, /*done=*/ false);
                batch = {};
                batch.reserve (kBatchSize);
            }
        }

        if (! threadShouldExit())
            postBatch (std::move (batch), found, /*done=*/ true);
    }

    void postBatch (std::vector<TrackFile> batch, int totalFound, bool done)
    {
        // Move-capture the batch into the lambda. owner outlives the
        // lambda (its destructor stops us first) so the reference
        // capture is safe.
        auto& ownerRef = owner;
        juce::MessageManager::callAsync (
            [&ownerRef, batch = std::move (batch), totalFound, done]() mutable
            {
                ownerRef.onScanBatchReady (std::move (batch), totalFound, done);
            });
    }

    MainComponent& owner;
    juce::File     targetFolder;
};

// =============================================================================
// SaveDebouncer: coalesces TrackStorage saves. A library broadcast (re)starts
// the timer; the actual snapshotAndSave fires once after kSaveDelayMs of
// quiet. Without this an 8 000-track JSON gets rewritten on every per-track
// lookup broadcast.
// =============================================================================
class MainComponent::SaveDebouncer : public juce::Timer
{
public:
    static constexpr int kSaveDelayMs = 750;

    explicit SaveDebouncer (MainComponent& o) : owner (o) {}

    void timerCallback() override
    {
        stopTimer();
        owner.flushSaveNow();
    }

    void schedule()
    {
        startTimer (kSaveDelayMs);
    }

    void cancel()
    {
        stopTimer();
    }

private:
    MainComponent& owner;
};

// =============================================================================
// SearchDebouncer: every keystroke restarts this timer; when it fires, the
// owner re-filters the visible-index list. ~120 ms feels live while keeping
// 8 000-track filtering smooth.
// =============================================================================
class MainComponent::SearchDebouncer : public juce::Timer
{
public:
    static constexpr int kSearchDelayMs = 120;

    explicit SearchDebouncer (MainComponent& o) : owner (o) {}

    void timerCallback() override
    {
        stopTimer();
        owner.applySearchNow();
    }

    void schedule() { startTimer (kSearchDelayMs); }
    void cancel()   { stopTimer(); }

private:
    MainComponent& owner;
};

MainComponent::MainComponent()
{
    // Log resolved data paths on every launch so Dave can paste them
    // into a terminal to inspect or wipe the cache without having to
    // guess where JUCE's userApplicationDataDirectory actually maps to.
    // (On macOS it's ~/Library, NOT ~/Library/Application Support —
    //  so the library + artwork cache live under ~/Library/My Tunes/
    //  even though that breaks Apple's convention. Renaming requires
    //  a one-shot migration; see the README note.)
    juce::Logger::writeToLog (
        "TuneList: library JSON = "
        + TrackStorage::getLibraryFile().getFullPathName());
    juce::Logger::writeToLog (
        "TuneList: artwork cache = "
        + ArtworkCache::getCacheFolder().getFullPathName());
    juce::Logger::writeToLog (
        "TuneList: metadata endpoint = " + metadataClient.getEndpoint());

    library.addChangeListener (this);

    // ---- toolbar -----------------------------------------------------------
    addAndMakeVisible (chooseFolderButton);
    chooseFolderButton.onClick = [this] { onChooseFolderClicked(); };

    addAndMakeVisible (scanButton);
    scanButton.onClick = [this] { runScan(); };
    scanButton.setEnabled (false);

    addAndMakeVisible (lookupCoversButton);
    lookupCoversButton.onClick = [this] { runManualLookup(); };
    lookupCoversButton.setEnabled (false);

    addAndMakeVisible (cancelButton);
    cancelButton.onClick = [this] { onCancelButtonClicked(); };
    cancelButton.setEnabled (false);

    addAndMakeVisible (viewListButton);
    viewListButton.setRadioGroupId (1);
    viewListButton.setClickingTogglesState (true);
    viewListButton.setToggleState (true, juce::dontSendNotification);
    viewListButton.onClick = [this] { setViewMode (ViewMode::List); };

    addAndMakeVisible (viewGridButton);
    viewGridButton.setRadioGroupId (1);
    viewGridButton.setClickingTogglesState (true);
    viewGridButton.onClick = [this] { setViewMode (ViewMode::Grid); };

    addAndMakeVisible (folderLabel);
    folderLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    folderLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));

    // ---- second toolbar row: search + diagnostics -------------------------
    addAndMakeVisible (searchField);
    searchField.setTextToShowWhenEmpty ("Search artist, title, filename...",
                                         juce::Colours::lightgrey.withAlpha (0.5f));
    searchField.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff262830));
    searchField.setColour (juce::TextEditor::textColourId,        juce::Colours::white);
    searchField.setColour (juce::TextEditor::outlineColourId,
                            juce::Colours::lightgrey.withAlpha (0.25f));
    searchField.setColour (juce::TextEditor::focusedOutlineColourId,
                            juce::Colour (0xff7d8db8));
    searchField.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));
    searchField.setEscapeAndReturnKeysConsumed (true);
    searchField.onTextChange = [this] { onSearchFieldChanged(); };
    // Escape key clears the field without losing focus.
    searchField.onEscapeKey  = [this] { onSearchClearClicked(); };

    addAndMakeVisible (searchClearButton);
    searchClearButton.setColour (juce::TextButton::buttonColourId,
                                  juce::Colour (0xff262830));
    searchClearButton.setColour (juce::TextButton::textColourOffId,
                                  juce::Colours::lightgrey);
    searchClearButton.onClick = [this] { onSearchClearClicked(); };
    searchClearButton.setEnabled (false);

    addAndMakeVisible (diagnosticsLabel);
    diagnosticsLabel.setJustificationType (juce::Justification::centredRight);
    diagnosticsLabel.setColour (juce::Label::textColourId,
                                juce::Colours::lightgrey.withAlpha (0.85f));
    diagnosticsLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));

    // ---- main area ---------------------------------------------------------
    addAndMakeVisible (emojiBoard);
    emojiBoard.onBoardClicked = [this] (int boardIndex) { onBoardClicked (boardIndex); };

    addAndMakeVisible (listView);
    addChildComponent  (gridView);
    listView.onTrackSelected       = [this] (int idx) { onTrackSelected (idx); };
    listView.onPlayToggleRequested = [this] (int idx) { togglePlayback   (idx); };
    gridView.onTrackSelected       = [this] (int idx) { onTrackSelected (idx); };
    gridView.onPlayToggleRequested = [this] (int idx) { togglePlayback   (idx); };

    previewPlayer.addChangeListener (this);

    addAndMakeVisible (detailView);
    addAndMakeVisible (miniPlayer);

    // Load any previously-persisted user metadata. Tracks are populated
    // on scan; storage gives them their tags / priority / notes /
    // Discogs metadata back.
    storage.load();

    // ---- footer ------------------------------------------------------------
    addAndMakeVisible (attributionLabel);
    // Plain ASCII only. Earlier builds used a curly U+2019 apostrophe
    // ("Discogs’") which renders as "Discogsâ€™" on some Mac setups
    // when the JUCE label paints under a non-UTF-8 font path.
    attributionLabel.setText ("Data and artwork may be provided by Spotify and Discogs.   "
                              "This application is not affiliated with, sponsored or "
                              "endorsed by Spotify or Discogs.",
                              juce::dontSendNotification);
    attributionLabel.setJustificationType (juce::Justification::centred);
    attributionLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
    attributionLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible (versionLabel);
    versionLabel.setText (MYTUNES_BUILD_LABEL, juce::dontSendNotification);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 10.5f, 0)));
    versionLabel.setColour (juce::Label::textColourId,
                            juce::Colours::lightgrey.withAlpha (0.65f));

    // ---- lookup wiring -----------------------------------------------------
    lookupController.onProgress = [this] (int done, int total)
    {
        lookupDone  = done;
        lookupTotal = total;
        updateStatusLabel();
    };
    lookupController.onTrackProcessed = [this] (int idx,
                                                 const LookupController::TrackOutcome& o)
    {
        onTrackLookupProcessed (idx, o);
    };
    lookupController.onFinished = [this]
    {
        isLookingUp = false;
        lookupDone  = 0;
        lookupTotal = 0;
        updateButtonsEnabled();
        updateStatusLabel();
        updateDiagnosticsLabel();
    };

    folderScanner   = std::make_unique<FolderScanner>   (*this);
    saveDebouncer   = std::make_unique<SaveDebouncer>   (*this);
    searchDebouncer = std::make_unique<SearchDebouncer> (*this);

    updateFolderLabel();
    updateStatusLabel();
    updateDiagnosticsLabel();
    updateButtonsEnabled();
    recomputeBoardCounts();

    setSize (1200, 800);
}

MainComponent::~MainComponent()
{
    // Stop background work BEFORE the library + storage members go
    // away, otherwise the worker thread can write into a half-
    // destructed library.
    if (folderScanner)   folderScanner->stopThread (4000);
    lookupController.cancel();
    if (saveDebouncer)   saveDebouncer->cancel();
    if (searchDebouncer) searchDebouncer->cancel();
    // Final synchronous save so anything still pending lands on disk.
    flushSaveNow();

    previewPlayer.removeChangeListener (this);
    library.removeChangeListener (this);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a1f));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Footer (two slots: centred attribution + right-side version label)
    {
        auto footer = bounds.removeFromBottom (28).reduced (12, 4);
        const int versionWidth = 220;
        if (footer.getWidth() > versionWidth + 100)
        {
            versionLabel.setBounds     (footer.removeFromRight (versionWidth));
            attributionLabel.setBounds (footer);
        }
        else
        {
            attributionLabel.setBounds (footer);
            versionLabel.setBounds ({});
        }
    }

    // Mini player strip (just above the footer). The strip is a
    // two-row layout (scrub bar + centred transport button), so it
    // needs more vertical room than the previous one-row version.
    miniPlayer.setBounds (bounds.removeFromBottom (66));

    // Top toolbar (44 tall)
    auto toolbar = bounds.removeFromTop (44).reduced (12, 6);
    chooseFolderButton.setBounds  (toolbar.removeFromLeft (140));
    toolbar.removeFromLeft (8);
    scanButton.setBounds          (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (8);
    lookupCoversButton.setBounds  (toolbar.removeFromLeft (110));
    toolbar.removeFromLeft (8);
    cancelButton.setBounds        (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (12);

    statusLabel.setBounds         (toolbar.removeFromRight (320));
    toolbar.removeFromRight (8);

    auto toggleArea = toolbar.removeFromRight (130);
    viewGridButton.setBounds (toggleArea.removeFromRight (60));
    viewListButton.setBounds (toggleArea.removeFromRight (60));
    toolbar.removeFromRight (8);

    folderLabel.setBounds         (toolbar);

    // Diagnostics row (24 tall, just below the toolbar)
    // Search field on the left, diagnostics text on the right.
    auto diagRow = bounds.removeFromTop (24).reduced (12, 1);
    auto searchSlot = diagRow.removeFromLeft (320);
    searchClearButton.setBounds (searchSlot.removeFromRight (22).reduced (0, 1));
    searchSlot.removeFromRight (4);
    searchField.setBounds       (searchSlot);
    diagRow.removeFromLeft (12);
    diagnosticsLabel.setBounds  (diagRow);

    // Main area
    bounds.removeFromTop (4);
    bounds.removeFromBottom (4);

    auto sidebar = bounds.removeFromLeft (240);
    auto detail  = bounds.removeFromRight (320);
    emojiBoard.setBounds (sidebar.reduced (8));
    detailView.setBounds (detail.reduced (8));

    auto centre = bounds.reduced (4);
    listView.setBounds (centre);
    gridView.setBounds (centre);
}

// =============================================================================
// Folder picking + scanning + auto-lookup
// =============================================================================

void MainComponent::onChooseFolderClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose a folder of tunes",
        getDefaultStartFolder(),
        juce::String());

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (folder == juce::File{} || ! folder.isDirectory())
            return;

        startFolderScan (folder);
    });
}

void MainComponent::runScan()
{
    if (! library.getFolder().isDirectory())
        return;
    startFolderScan (library.getFolder());
}

void MainComponent::startFolderScan (juce::File folder)
{
    // Cancel anything still in flight so we don't apply stale batches
    // / stale lookup results to the new library state.
    cancelInFlightWork();

    library.clear (folder);

    scanFolder  = std::move (folder);
    isScanning  = true;
    scanFound   = 0;

    updateFolderLabel();
    updateButtonsEnabled();
    updateStatusLabel();

    folderScanner->scan (scanFolder);
}

void MainComponent::cancelInFlightWork()
{
    if (folderScanner)
        folderScanner->stopThread (4000);
    lookupController.cancel();

    isScanning  = false;
    isLookingUp = false;
    lookupDone  = 0;
    lookupTotal = 0;
    updateButtonsEnabled();
    updateStatusLabel();
    updateDiagnosticsLabel();
}

void MainComponent::onCancelButtonClicked()
{
    cancelInFlightWork();
}

void MainComponent::onScanBatchReady (std::vector<TrackFile> batch,
                                      int totalFound, bool done)
{
    library.appendBatchSilently (std::move (batch));
    scanFound = totalFound;

    if (! done)
    {
        // Mid-scan: just update the status label. The views will
        // catch up once the scan completes and we fire a real
        // library broadcast.
        updateStatusLabel();
        return;
    }

    // Scan complete — apply persisted tags / metadata and fire the
    // one-shot library broadcast that updates List, Grid, sidebar
    // counts, and triggers a debounced save.
    isScanning = false;
    storage.applySavedFieldsToLibrary (library);
    library.sendChangeMessage();

    scanButton.setEnabled (true);
    updateButtonsEnabled();
    updateStatusLabel();

    // Auto-fire the metadata lookup for the freshly-scanned set.
    // LookupController already (a) runs on a background thread,
    // (b) skips tracks that already have cached artwork, and
    // (c) paces requests so a large folder doesn't hammer the proxy.
    startAutoLookup();
}

void MainComponent::startAutoLookup()
{
    if (lookupController.isRunning() || library.getNumTracks() == 0)
        return;

    std::vector<int> indices;
    const int n = library.getNumTracks();
    indices.reserve ((std::size_t) n);
    for (int i = 0; i < n; ++i)
        indices.push_back (i);

    if (indices.empty()) return;

    isLookingUp = true;
    lookupDone  = 0;
    lookupTotal = (int) indices.size();
    resetLookupCounters();
    updateButtonsEnabled();
    updateStatusLabel();
    updateDiagnosticsLabel();

    lookupController.startLookup (indices, /*forceRelookup*/ false);
}

void MainComponent::runManualLookup()
{
    // Re-lookup button: force-refresh artwork for every track. With
    // Spotify creds present this is how Dave promotes an old Discogs-
    // sourced cover to the new Spotify cover.
    if (lookupController.isRunning() || library.getNumTracks() == 0)
        return;

    std::vector<int> indices;
    const int n = library.getNumTracks();
    indices.reserve ((std::size_t) n);
    for (int i = 0; i < n; ++i)
        indices.push_back (i);

    isLookingUp = true;
    lookupDone  = 0;
    lookupTotal = (int) indices.size();
    resetLookupCounters();
    updateButtonsEnabled();
    updateStatusLabel();
    updateDiagnosticsLabel();

    lookupController.startLookup (indices, /*forceRelookup*/ true);
}

void MainComponent::onTrackLookupProcessed (int /*trackIndex*/,
                                             const LookupController::TrackOutcome& o)
{
    switch (o.artworkSource)
    {
        case TrackFile::ArtworkSource::Spotify: ++countSpotifyArt; break;
        case TrackFile::ArtworkSource::Discogs: ++countDiscogsArt; break;
        case TrackFile::ArtworkSource::None:    ++countNoArtwork;  break;
    }

    if (o.rejectedByLocalScoring)
        ++countRejected;

    if (o.proxySource == MetadataProxyClient::Source::Error)
    {
        ++countLookupError;
        if (o.proxyError.isNotEmpty())
            lastLookupError = o.proxyError;
    }

    updateDiagnosticsLabel();
}

// =============================================================================
// Reactions to library changes
// =============================================================================

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    // Library changes -> persist state, but DEBOUNCED. With thousands
    // of tracks the JSON write is in the tens-of-ms range and the
    // lookup pipeline broadcasts one library change per track —
    // saving on every broadcast was the dominant freeze cause at
    // 8 000 tracks. The debouncer waits ~750 ms after the last
    // broadcast before writing once.
    if (src == &library)
        scheduleSave();

    recomputeVisibleIndices();
    recomputeBoardCounts();
    updateFolderLabel();
    updateStatusLabel();
    updateDiagnosticsLabel();
    updateButtonsEnabled();
}

void MainComponent::scheduleSave()
{
    if (saveDebouncer)
        saveDebouncer->schedule();
}

void MainComponent::flushSaveNow()
{
    if (library.getNumTracks() > 0 || library.getFolder().isDirectory())
        storage.snapshotAndSave (library);
}

void MainComponent::onSearchFieldChanged()
{
    searchClearButton.setEnabled (searchField.getText().isNotEmpty());
    if (searchDebouncer)
        searchDebouncer->schedule();
}

void MainComponent::onSearchClearClicked()
{
    searchField.setText ({}, juce::sendNotificationSync);
    searchField.giveAwayKeyboardFocus();
    // setText fires onTextChange which schedules a rebuild; also
    // explicitly apply now in case the field was already empty.
    applySearchNow();
}

void MainComponent::applySearchNow()
{
    searchText = searchField.getText().trim().toLowerCase();
    recomputeVisibleIndices();
    updateStatusLabel();
}

void MainComponent::togglePlayback (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= library.getNumTracks())
        return;

    if (previewPlayer.getPlayingTrackIndex() == trackIndex)
    {
        previewPlayer.stop();
        return;
    }

    const auto& tf = library.getTrack (trackIndex);
    if (! tf.file.existsAsFile())
        return;

    previewPlayer.play (trackIndex, tf.file);
}

// =============================================================================
// View mode + filter
// =============================================================================

void MainComponent::setViewMode (ViewMode mode)
{
    viewMode = mode;
    listView.setVisible (mode == ViewMode::List);
    gridView.setVisible (mode == ViewMode::Grid);

    viewListButton.setToggleState (mode == ViewMode::List, juce::dontSendNotification);
    viewGridButton.setToggleState (mode == ViewMode::Grid, juce::dontSendNotification);
}

void MainComponent::onBoardClicked (int boardIndex)
{
    if (boardIndex == activeBoardIndex)
        activeBoardIndex = -1;
    else
        activeBoardIndex = boardIndex;

    emojiBoard.setActiveBoardIndex (activeBoardIndex);
    recomputeVisibleIndices();
    updateStatusLabel();
}

void MainComponent::onTrackSelected (int trackIndex)
{
    selectedTrackIndex = trackIndex;
    detailView.setSelectedTrackIndex (trackIndex);
}

// =============================================================================
// Recomputes
// =============================================================================

void MainComponent::recomputeVisibleIndices()
{
    visibleIndices.clear();
    const int n = library.getNumTracks();

    const bool emojiActive  = activeBoardIndex >= 0;
    const bool searchActive = searchText.isNotEmpty();

    if (! emojiActive && ! searchActive)
    {
        // Sentinel: an empty visibleIndices vector tells the list +
        // grid views to "show everything" — they avoid an O(n) copy
        // by skipping the filter entirely.
        listView.setVisibleTrackIndices (visibleIndices);
        gridView.setVisibleTrackIndices (visibleIndices);
        return;
    }

    juce::String boardEmoji;
    if (emojiActive)
    {
        const auto& boards = EmojiTagBoardComponent::getBoards();
        boardEmoji = boards[(std::size_t) activeBoardIndex].emoji;
    }

    visibleIndices.reserve ((std::size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const auto& tf = library.getTrack (i);
        if (emojiActive && ! tf.emojiTags.contains (boardEmoji))
            continue;
        if (searchActive && ! tf.searchKey.contains (searchText))
            continue;
        visibleIndices.push_back (i);
    }

    listView.setVisibleTrackIndices (visibleIndices);
    gridView.setVisibleTrackIndices (visibleIndices);

    // If the currently selected track no longer survives the filter,
    // drop the selection so the detail panel doesn't keep pointing at
    // an invisible row.
    if (selectedTrackIndex >= 0)
    {
        const auto& tf = library.getTrack (selectedTrackIndex);
        const bool stillVisible =
            (! emojiActive  || tf.emojiTags.contains (boardEmoji))
         && (! searchActive || tf.searchKey.contains (searchText));
        if (! stillVisible)
            onTrackSelected (-1);
    }
}

void MainComponent::recomputeBoardCounts()
{
    const auto& boards = EmojiTagBoardComponent::getBoards();
    for (int i = 0; i < (int) boards.size(); ++i)
        emojiBoard.setBoardCount (i, library.countTracksWithTag (boards[(std::size_t) i].emoji));
}

void MainComponent::resetLookupCounters()
{
    countSpotifyArt = countDiscogsArt = countNoArtwork = 0;
    countRejected = countLookupError = 0;
    lastLookupError.clear();
}

void MainComponent::updateFolderLabel()
{
    const auto f = library.getFolder();
    folderLabel.setText (f == juce::File{} ? juce::String ("(no folder selected)")
                                            : f.getFullPathName(),
                         juce::dontSendNotification);
}

void MainComponent::updateStatusLabel()
{
    // Helper for "12,345" formatting on the bigger numbers Dave sees
    // with multi-thousand libraries. juce::String doesn't ship a
    // locale-aware formatter so we insert commas by hand.
    auto withCommas = [] (int x) -> juce::String
    {
        const auto raw = juce::String (x);
        juce::String out;
        const int len = raw.length();
        for (int i = 0; i < len; ++i)
        {
            if (i > 0 && (len - i) % 3 == 0) out << ',';
            out << raw.substring (i, i + 1);
        }
        return out;
    };

    const int n = library.getNumTracks();

    juce::String text;

    // Top priority: playback state.
    if (previewPlayer.isPlaying())
    {
        const int idx = previewPlayer.getPlayingTrackIndex();
        if (idx >= 0 && idx < library.getNumTracks())
        {
            const auto& tf = library.getTrack (idx);
            text << "Playing: " << tf.artist
                 << (tf.title.isNotEmpty() ? juce::String (" - ") + tf.title
                                           : juce::String());
        }
        else
        {
            text << "Playing";
        }
        statusLabel.setText (text, juce::dontSendNotification);
        return;
    }

    if (isScanning)
    {
        text << "Scanning... " << withCommas (scanFound) << " tracks found";
    }
    else
    {
        text << "Loaded " << withCommas (n) << (n == 1 ? " track" : " tracks");
    }

    // Search + emoji-filter summary. Shown whenever either filter is
    // active so the user can tell how many of the loaded tracks are
    // currently visible.
    const bool emojiActive  = activeBoardIndex >= 0;
    const bool searchActive = searchText.isNotEmpty();
    if (emojiActive || searchActive)
    {
        const int shown = (int) visibleIndices.size();
        text << "   |   Showing " << withCommas (shown);
        if (searchActive)
            text << " matching \"" << searchText << "\"";
        if (emojiActive)
        {
            const auto& boards = EmojiTagBoardComponent::getBoards();
            const auto& b      = boards[(std::size_t) activeBoardIndex];
            text << (searchActive ? " in " : " in ") << b.emoji << " " << b.label;
        }
    }

    if (isLookingUp && lookupTotal > 0)
    {
        text << "   |   Artwork lookup "
             << withCommas (lookupDone) << "/" << withCommas (lookupTotal);
    }
    else if (countSpotifyArt + countDiscogsArt + countNoArtwork > 0)
    {
        text << "   |   Artwork: "
             << withCommas (countSpotifyArt) << " Spotify, "
             << withCommas (countDiscogsArt) << " Discogs, "
             << withCommas (countNoArtwork)  << " none";
    }
    statusLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::updateDiagnosticsLabel()
{
    // User-facing artwork summary. Deliberately does NOT mention the
    // Cloud Run endpoint or any other backend internals — those live
    // in the startup log only.
    juce::String text;

    if (isLookingUp)
    {
        text << "Artwork: looking up " << lookupDone << "/" << lookupTotal;
    }
    else if (lookupTotal == 0
             && (countSpotifyArt + countDiscogsArt + countNoArtwork) == 0)
    {
        text << "Artwork: idle";
    }
    else
    {
        text << "Artwork: "
             << countSpotifyArt << " Spotify, "
             << countDiscogsArt << " Discogs, "
             << countNoArtwork  << " none";
        if (countRejected > 0)
            text << ", " << countRejected << " rejected";
        if (countLookupError > 0)
            text << ", " << countLookupError << " errors";
    }

    if (lastLookupError.isNotEmpty())
        text << "   |   last error: " << lastLookupError;

    diagnosticsLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::updateButtonsEnabled()
{
    const bool haveTracks    = library.getNumTracks() > 0;
    const bool workInFlight  = isScanning || isLookingUp;

    chooseFolderButton.setEnabled (! isScanning);
    scanButton.setEnabled         (library.getFolder().isDirectory() && ! workInFlight);
    lookupCoversButton.setEnabled (haveTracks && ! workInFlight);
    cancelButton.setEnabled       (workInFlight);
}

juce::File MainComponent::getDefaultStartFolder()
{
    const juce::File daveExample
        ("/Volumes/Data/Tunes/Dave's Tunes 2020 [KEYED WITH SWIFT APP]");
    if (daveExample.isDirectory())
        return daveExample;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}
