#include "TrackDetailComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"

TrackDetailComponent::TrackDetailComponent (TrackLibrary& l) : library (l)
{
    library.addChangeListener (this);
}

TrackDetailComponent::~TrackDetailComponent()
{
    library.removeChangeListener (this);
}

void TrackDetailComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void TrackDetailComponent::resized() {}

void TrackDetailComponent::setSelectedTrackIndex (int index)
{
    if (selectedIndex == index) return;
    selectedIndex = index;
    repaint();
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
        g.drawText  (value.isEmpty() ? juce::String ("—") : value,
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
                                    : juce::String ("(unset, 1–5)"));

    drawLabelValue (g, bounds, "Notes",
                    tf.notes.isEmpty() ? juce::String ("(empty)") : tf.notes);

    drawLabelValue (g, bounds, "Discogs",
                    TrackFile::discogsStatusText (tf.discogsStatus));

    if (tf.discogsStatus == TrackFile::DiscogsStatus::Found)
    {
        if (tf.discogsArtist.isNotEmpty() || tf.discogsTitle.isNotEmpty())
            drawLabelValue (g, bounds, "Matched",
                            tf.discogsArtist + " — " + tf.discogsTitle);
        if (tf.discogsReleaseId.isNotEmpty())
            drawLabelValue (g, bounds, "Release ID", tf.discogsReleaseId);
    }
}
