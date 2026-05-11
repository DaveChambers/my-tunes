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
    openSpotifyButton.onClick = [this] { onOpenSpotifyClicked(); };
    refreshOpenSpotifyButton();
}

TrackDetailComponent::~TrackDetailComponent()
{
    library.removeChangeListener (this);
    player .removeChangeListener (this);
}

void TrackDetailComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshOpenSpotifyButton();
    repaint();
}

void TrackDetailComponent::resized()
{
    // Button sits in a 28px strip at the bottom of the panel. It's
    // only visible when the current track has a Spotify external URL.
    auto bounds = getLocalBounds().reduced (12);
    auto buttonRow = bounds.removeFromBottom (28);
    openSpotifyButton.setBounds (buttonRow.removeFromLeft (140));
}

void TrackDetailComponent::setSelectedTrackIndex (int index)
{
    if (selectedIndex == index) return;
    selectedIndex = index;
    refreshOpenSpotifyButton();
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

void TrackDetailComponent::refreshOpenSpotifyButton()
{
    const bool show = selectedIndex >= 0
                       && selectedIndex < library.getNumTracks()
                       && library.getTrack (selectedIndex)
                                  .spotifyExternalUrl.isNotEmpty();
    openSpotifyButton.setVisible (show);
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

    drawLabelValue (g, bounds, "Spotify",
                    TrackFile::spotifyStatusText (tf.spotifyStatus));
    if (tf.spotifyStatus == TrackFile::SpotifyStatus::Found
        || tf.spotifyStatus == TrackFile::SpotifyStatus::Uncertain)
    {
        if (tf.spotifyArtist.isNotEmpty() || tf.spotifyTitle.isNotEmpty())
            drawLabelValue (g, bounds, "Spotify match",
                            tf.spotifyArtist + "  |  " + tf.spotifyTitle);
    }
    if (tf.spotifyStatus == TrackFile::SpotifyStatus::Error
        && tf.spotifyErrorReason.isNotEmpty())
    {
        drawLabelValue (g, bounds, "Spotify error", tf.spotifyErrorReason);
    }

    drawLabelValue (g, bounds, "Discogs",
                    TrackFile::discogsStatusText (tf.discogsStatus));

    if (tf.discogsStatus == TrackFile::DiscogsStatus::Found)
    {
        if (tf.discogsArtist.isNotEmpty() || tf.discogsTitle.isNotEmpty())
            drawLabelValue (g, bounds, "Discogs match",
                            tf.discogsArtist + "  |  " + tf.discogsTitle);
        if (tf.discogsReleaseId.isNotEmpty())
            drawLabelValue (g, bounds, "Release ID", tf.discogsReleaseId);
    }

    if (tf.discogsStatus == TrackFile::DiscogsStatus::Error
        && tf.discogsErrorReason.isNotEmpty())
    {
        drawLabelValue (g, bounds, "Discogs error", tf.discogsErrorReason);
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
