#include "MainComponent.h"

MainComponent::MainComponent()
{
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

    // ---- second toolbar row: diagnostics ----------------------------------
    addAndMakeVisible (diagnosticsLabel);
    diagnosticsLabel.setJustificationType (juce::Justification::centredLeft);
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
    attributionLabel.setText ("Data provided by Discogs.   "
                              "This application uses the Discogs API but is not "
                              "affiliated with, sponsored or endorsed by Discogs.",
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
                                                 const DiscogsClient::Result& r)
    {
        onTrackLookupProcessed (idx, r);
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

    updateFolderLabel();
    updateStatusLabel();
    updateDiagnosticsLabel();
    updateButtonsEnabled();
    recomputeBoardCounts();

    setSize (1200, 800);
}

MainComponent::~MainComponent()
{
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
    toolbar.removeFromLeft (12);

    statusLabel.setBounds         (toolbar.removeFromRight (320));
    toolbar.removeFromRight (8);

    auto toggleArea = toolbar.removeFromRight (130);
    viewGridButton.setBounds (toggleArea.removeFromRight (60));
    viewListButton.setBounds (toggleArea.removeFromRight (60));
    toolbar.removeFromRight (8);

    folderLabel.setBounds         (toolbar);

    // Diagnostics row (24 tall, just below the toolbar)
    auto diagRow = bounds.removeFromTop (22);
    diagnosticsLabel.setBounds (diagRow.reduced (12, 2));

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

        library.scanFolder (folder, kSampleSize);
        // Re-apply any previously-saved tags / priority / notes / Discogs
        // metadata to the freshly-scanned tracks before the lookup
        // starts (so already-known matches don't get re-queried).
        storage.applySavedFieldsToLibrary (library);
        library.sendChangeMessage();
        scanButton.setEnabled (true);

        // Auto-fire Discogs lookup for the new sample. The user shouldn't
        // have to find a button to make this happen.
        startAutoLookup();
    });
}

void MainComponent::runScan()
{
    if (! library.getFolder().isDirectory())
        return;

    library.scanFolder (library.getFolder(), kSampleSize);
    storage.applySavedFieldsToLibrary (library);
    library.sendChangeMessage();
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
    resetDiscogsCounters();
    updateButtonsEnabled();
    updateStatusLabel();
    updateDiagnosticsLabel();

    lookupController.startLookup (indices);
}

void MainComponent::runManualLookup()
{
    // Same path as auto, just driven by the toolbar button. Useful for
    // re-running after a connection / token change.
    startAutoLookup();
}

void MainComponent::onTrackLookupProcessed (int /*trackIndex*/,
                                             const DiscogsClient::Result& r)
{
    using S = TrackFile::DiscogsStatus;
    switch (r.status)
    {
        case S::Found:      ++countFound;     break;
        case S::Uncertain:  ++countUncertain; break;
        case S::NotFound:   ++countNotFound;  break;
        case S::Error:      ++countError;     break;
        default:            break;
    }

    if (r.errorReason.isNotEmpty())
        lastDiscogsError = r.errorReason;

    updateDiagnosticsLabel();
}

// =============================================================================
// Reactions to library changes
// =============================================================================

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    // Library changes -> persist state. Player changes don't affect
    // persisted data, so we only save on library changes. Saving 20
    // tracks of JSON to ~/Library/Application Support takes
    // sub-millisecond on a local SSD; doing it on every library
    // broadcast (incl. each Discogs lookup result) is fine for v0.1.
    if (src == &library)
        storage.snapshotAndSave (library);

    recomputeVisibleIndices();
    recomputeBoardCounts();
    updateFolderLabel();
    updateStatusLabel();
    updateDiagnosticsLabel();
    updateButtonsEnabled();
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

    if (activeBoardIndex >= 0)
    {
        const auto& boards = EmojiTagBoardComponent::getBoards();
        const auto  emoji  = boards[(std::size_t) activeBoardIndex].emoji;
        for (int i = 0; i < n; ++i)
            if (library.getTrack (i).emojiTags.contains (emoji))
                visibleIndices.push_back (i);
    }

    listView.setVisibleTrackIndices (visibleIndices);
    gridView.setVisibleTrackIndices (visibleIndices);

    if (selectedTrackIndex >= 0 && activeBoardIndex >= 0)
    {
        const auto& boards = EmojiTagBoardComponent::getBoards();
        const auto  emoji  = boards[(std::size_t) activeBoardIndex].emoji;
        if (! library.getTrack (selectedTrackIndex).emojiTags.contains (emoji))
            onTrackSelected (-1);
    }
}

void MainComponent::recomputeBoardCounts()
{
    const auto& boards = EmojiTagBoardComponent::getBoards();
    for (int i = 0; i < (int) boards.size(); ++i)
        emojiBoard.setBoardCount (i, library.countTracksWithTag (boards[(std::size_t) i].emoji));
}

void MainComponent::resetDiscogsCounters()
{
    countFound = countUncertain = countNotFound = countError = 0;
    lastDiscogsError.clear();
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

    text << "Sample mode: " << kSampleSize
         << "   |   "        << n << " loaded";

    if (isLookingUp && lookupTotal > 0)
    {
        text << "   |   Discogs lookup " << lookupDone << "/" << lookupTotal;
    }
    else if (countFound + countUncertain + countNotFound + countError > 0)
    {
        text << "   |   Discogs complete: "
             << countFound     << " found, "
             << countUncertain << " uncertain, "
             << countNotFound  << " not found";
        if (countError > 0)
            text << ", " << countError << " error";
    }
    else if (activeBoardIndex >= 0)
    {
        const auto& boards = EmojiTagBoardComponent::getBoards();
        const auto& b      = boards[(std::size_t) activeBoardIndex];
        text << "   |   filtered: " << b.emoji << " " << b.label
             << " (" << (int) visibleIndices.size() << ")";
    }

    statusLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::updateDiagnosticsLabel()
{
    juce::String text;
    text << "Discogs: "
         << (discogsClient.hasToken() ? "token" : "no token (unauth)")
         << "   |   ";

    if (isLookingUp)
        text << "lookup running";
    else if (lookupTotal == 0 && (countFound + countUncertain + countNotFound + countError) == 0)
        text << "lookup idle";
    else
        text << "lookup complete";

    if (lastDiscogsError.isNotEmpty())
        text << "   |   last error: " << lastDiscogsError;

    diagnosticsLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::updateButtonsEnabled()
{
    const bool haveTracks = library.getNumTracks() > 0;
    scanButton.setEnabled (library.getFolder().isDirectory());
    lookupCoversButton.setEnabled (haveTracks && ! isLookingUp);
}

juce::File MainComponent::getDefaultStartFolder()
{
    const juce::File daveExample
        ("/Volumes/Data/Tunes/Dave's Tunes 2020 [KEYED WITH SWIFT APP]");
    if (daveExample.isDirectory())
        return daveExample;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}
