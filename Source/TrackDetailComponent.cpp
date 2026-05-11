#include "TrackDetailComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

TrackDetailComponent::TrackDetailComponent (TrackLibrary& l, TrackPreviewPlayer& p)
    : library (l), player (p)
{
    library.addChangeListener (this);
    player .addChangeListener (this);

    addChildComponent (openSpotifyButton);
    addChildComponent (openDiscogsButton);
    openSpotifyButton.onClick = [this] { onOpenSpotifyClicked(); };
    openDiscogsButton.onClick = [this] { onOpenDiscogsClicked(); };
    refreshExternalLinkButtons();
}

TrackDetailComponent::~TrackDetailComponent()
{
    library.removeChangeListener (this);
    player .removeChangeListener (this);
}

void TrackDetailComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshExternalLinkButtons();
    repaint();
}

void TrackDetailComponent::resized()
{
    // 28 px strip at the bottom of the panel for whichever
    // external-link buttons are visible. Each shown button takes a
    // 140 px slot from the left, with an 8 px gap. Hidden buttons
    // collapse cleanly.
    auto bounds    = getLocalBounds().reduced (12);
    auto buttonRow = bounds.removeFromBottom (28);
    if (openSpotifyButton.isVisible())
    {
        openSpotifyButton.setBounds (buttonRow.removeFromLeft (140));
        buttonRow.removeFromLeft (8);
    }
    if (openDiscogsButton.isVisible())
    {
        openDiscogsButton.setBounds (buttonRow.removeFromLeft (140));
    }
}

void TrackDetailComponent::setSelectedTrackIndex (int index)
{
    if (selectedIndex == index) return;
    selectedIndex = index;
    refreshExternalLinkButtons();
    repaint();
}

void TrackDetailComponent::onOpenSpotifyClicked()
{
    if (selectedIndex < 0 || selectedIndex >= library.getNumTracks())
        return;
    const auto& tf = library.getTrack (selectedIndex);
    if (tf.spotifyExternalUrl.isEmpty()) return;
    juce::URL (tf.spotifyExternalUrl).launchInDefaultBrowser();
}

void TrackDetailComponent::onOpenDiscogsClicked()
{
    if (selectedIndex < 0 || selectedIndex >= library.getNumTracks())
        return;
    const auto& tf = library.getTrack (selectedIndex);
    if (tf.discogsExternalUrl.isEmpty()) return;
    juce::URL (tf.discogsExternalUrl).launchInDefaultBrowser();
}

void TrackDetailComponent::refreshExternalLinkButtons()
{
    const bool haveTrack = selectedIndex >= 0
                            && selectedIndex < library.getNumTracks();
    const auto& tf = haveTrack ? library.getTrack (selectedIndex) : TrackFile{};
    openSpotifyButton.setVisible (haveTrack && tf.spotifyExternalUrl.isNotEmpty());
    openDiscogsButton.setVisible (haveTrack && tf.discogsExternalUrl.isNotEmpty());
    // Re-layout the bottom row in case visibility changed.
    resized();
}

namespace
{
    void drawLabelValue (juce::Graphics& g,
                         juce::Rectangle<int>& bounds,
                         const juce::String& label,
                         const juce::String& value)
    {
        auto row = bounds.removeFromTop (38);
        bounds.removeFromTop (2);

        g.setColour (juce::Colours::lightgrey);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 10.0f, 0)));
        g.drawText  (label.toUpperCase(),
                     row.removeFromTop (14),
                     juce::Justification::topLeft);

        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 13.0f, 0)));
        g.drawText  (value.isEmpty() ? juce::String ("-") : value,
                     row,
                     juce::Justification::topLeft,
                     /*useEllipsesIfTooBig=*/ true);
    }
}

void TrackDetailComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1c22));

    auto bounds = getLocalBounds().reduced (12);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 14.0f, juce::Font::bold)));
    g.drawText  ("Details",
                 bounds.removeFromTop (20),
                 juce::Justification::topLeft);
    bounds.removeFromTop (8);

    if (selectedIndex < 0 || selectedIndex >= library.getNumTracks())
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));
        g.drawText  ("(no track selected)", bounds, juce::Justification::centredTop);
        return;
    }

    const auto& tf = library.getTrack (selectedIndex);

    // Cover art at the top, if we have one cached.
    if (tf.cachedArtworkPath.existsAsFile())
    {
        const auto art = juce::ImageCache::getFromFile (tf.cachedArtworkPath);
        if (art.isValid())
        {
            auto coverArea = bounds.removeFromTop (160);
            g.setColour (juce::Colour (0xff14151a));
            g.fillRoundedRectangle (coverArea.toFloat(), 4.0f);
            g.drawImageWithin (art,
                               coverArea.getX(), coverArea.getY(),
                               coverArea.getWidth(), coverArea.getHeight(),
                               juce::RectanglePlacement::centred,
                               false);
            bounds.removeFromTop (8);
        }
    }

    drawLabelValue (g, bounds, "Filename", tf.filename);
    drawLabelValue (g, bounds, "Artist",   tf.artist);
    drawLabelValue (g, bounds, "Title",    tf.title);
    drawLabelValue (g, bounds, "Version",  tf.version);

    juce::String keyBpm;
    if (tf.key.isNotEmpty()) keyBpm << tf.key;
    if (tf.bpm > 0)
    {
        if (keyBpm.isNotEmpty()) keyBpm << "   ";
        keyBpm << juce::String (tf.bpm) << " BPM";
    }
    drawLabelValue (g, bounds, "Key / BPM", keyBpm);

    drawLabelValue (g, bounds, "Tags",
                    tf.emojiTags.isEmpty() ? juce::String()
                                           : tf.emojiTags.joinIntoString ("  "));

    drawLabelValue (g, bounds, "Priority",
                    tf.priority > 0 ? juce::String (tf.priority)
                                    : juce::String ("(unset, 1-5)"));

    drawLabelValue (g, bounds, "Notes",
                    tf.notes.isEmpty() ? juce::String ("(empty)") : tf.notes);

    drawLabelValue (g, bounds, "Artwork source",
                    TrackFile::artworkSourceText (tf.artworkSource));
    if (tf.metadataQuery.isNotEmpty())
        drawLabelValue (g, bounds, "Lookup query", tf.metadataQuery);

    // Spotify match info (shown when the proxy chose Spotify).
    if (tf.artworkSource == TrackFile::ArtworkSource::Spotify
        || tf.spotifyTitle.isNotEmpty())
    {
        if (tf.spotifyArtist.isNotEmpty())
            drawLabelValue (g, bounds, "Spotify artist", tf.spotifyArtist);
        if (tf.spotifyTitle.isNotEmpty())
            drawLabelValue (g, bounds, "Spotify title",  tf.spotifyTitle);
        if (tf.spotifyAlbum.isNotEmpty())
            drawLabelValue (g, bounds, "Spotify album",  tf.spotifyAlbum);
        if (tf.spotifyExternalUrl.isNotEmpty())
            drawLabelValue (g, bounds, "Spotify URL",    tf.spotifyExternalUrl);
    }

    // Discogs match info (shown when the proxy chose Discogs).
    if (tf.artworkSource == TrackFile::ArtworkSource::Discogs
        || tf.discogsTitle.isNotEmpty())
    {
        if (tf.discogsArtist.isNotEmpty())
            drawLabelValue (g, bounds, "Discogs artist", tf.discogsArtist);
        if (tf.discogsTitle.isNotEmpty())
            drawLabelValue (g, bounds, "Discogs title",  tf.discogsTitle);
        if (tf.discogsReleaseId.isNotEmpty())
            drawLabelValue (g, bounds, "Discogs release", tf.discogsReleaseId);
        if (tf.discogsExternalUrl.isNotEmpty())
            drawLabelValue (g, bounds, "Discogs URL",    tf.discogsExternalUrl);
    }

    if (tf.artworkSource == TrackFile::ArtworkSource::None
        && tf.artworkFallbackReason.isNotEmpty())
    {
        drawLabelValue (g, bounds, "Fallback reason", tf.artworkFallbackReason);
    }

    // Playback row
    const bool isPlayingThis = (player.getPlayingTrackIndex() == selectedIndex);
    drawLabelValue (g, bounds, "Playback",
                    isPlayingThis ? juce::String ("Playing")
                                  : juce::String ("Stopped"));

    const auto err = player.getLastError();
    if (err.isNotEmpty() && ! isPlayingThis)
        drawLabelValue (g, bounds, "Playback error", err);
}
