#include "TrackGridComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

namespace
{
    constexpr int kCardW       = 200;
    constexpr int kCardH       = 240;
    constexpr int kGutter      = 12;
    constexpr int kOuterMargin = 12;
    constexpr int kPlayDiam    = 56;

    int columnsFor (int width)
    {
        const int usable = juce::jmax (kCardW, width - 2 * kOuterMargin);
        const int cols   = juce::jmax (1, (usable + kGutter) / (kCardW + kGutter));
        return cols;
    }

    void paintTransportOverlay (juce::Graphics& g,
                                juce::Rectangle<int> rect,
                                bool isPlaying)
    {
        const auto fb = rect.toFloat();
        // Semi-transparent disc
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (fb);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawEllipse (fb.reduced (1.0f), 1.0f);

        const float cx = fb.getCentreX();
        const float cy = fb.getCentreY();
        const float r  = fb.getWidth() * 0.22f;

        g.setColour (juce::Colours::white);
        if (isPlaying)
        {
            const float side = r * 1.6f;
            g.fillRect (juce::Rectangle<float> (cx - side / 2,
                                                 cy - side / 2,
                                                 side, side));
        }
        else
        {
            juce::Path p;
            p.startNewSubPath (cx - r * 0.55f, cy - r);
            p.lineTo          (cx - r * 0.55f, cy + r);
            p.lineTo          (cx + r,         cy);
            p.closeSubPath();
            g.fillPath (p);
        }
    }
}

TrackGridComponent::TrackGridComponent (TrackLibrary& l, TrackPreviewPlayer& p)
    : library (l), player (p)
{
    library.addChangeListener (this);
    player .addChangeListener (this);
}

TrackGridComponent::~TrackGridComponent()
{
    library.removeChangeListener (this);
    player .removeChangeListener (this);
}

void TrackGridComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void TrackGridComponent::resized()
{
    repaint();
}

void TrackGridComponent::refresh()
{
    repaint();
}

void TrackGridComponent::setVisibleTrackIndices (std::vector<int> indices)
{
    visibleIndices    = std::move (indices);
    selectedCardIndex = -1;
    refresh();
}

int TrackGridComponent::numVisible() const
{
    return visibleIndices.empty() ? library.getNumTracks()
                                  : (int) visibleIndices.size();
}

int TrackGridComponent::visibleAt (int n) const
{
    if (n < 0) return -1;
    if (visibleIndices.empty())
        return n < library.getNumTracks() ? n : -1;
    if (n < (int) visibleIndices.size())
        return visibleIndices[(std::size_t) n];
    return -1;
}

juce::Rectangle<int> TrackGridComponent::cardRect (int cardIndex,
                                                    int cardW, int cardH,
                                                    int cols) const
{
    const int row = cardIndex / cols;
    const int col = cardIndex % cols;
    const int x = kOuterMargin + col * (cardW + kGutter);
    const int y = kOuterMargin + row * (cardH + kGutter);
    return { x, y, cardW, cardH };
}

juce::Rectangle<int> TrackGridComponent::playHitRect (juce::Rectangle<int> coverArea) const
{
    return juce::Rectangle<int> (0, 0, kPlayDiam, kPlayDiam)
              .withCentre (coverArea.getCentre());
}

void TrackGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202229));

    const int n = numVisible();
    if (n == 0)
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 13.0f, 0)));
        g.drawText  ("(no tracks)", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const int cols = columnsFor (getWidth());

    const int playingIdx = player.getPlayingTrackIndex();

    for (int i = 0; i < n; ++i)
    {
        const int libIdx = visibleAt (i);
        if (libIdx < 0) continue;
        const auto rect    = cardRect (i, kCardW, kCardH, cols);
        const bool playing = (libIdx == playingIdx);
        paintCard (g, libIdx, rect, i == selectedCardIndex, playing);
    }
}

void TrackGridComponent::paintCard (juce::Graphics& g, int libraryIndex,
                                    juce::Rectangle<int> bounds, bool selected,
                                    bool isPlayingThis) const
{
    if (libraryIndex < 0 || libraryIndex >= library.getNumTracks())
        return;

    const auto& tf = library.getTrack (libraryIndex);

    g.setColour (selected ? juce::Colour (0xff3a4156) : juce::Colour (0xff262830));
    g.fillRoundedRectangle (bounds.toFloat(), 8.0f);

    if (selected)
    {
        g.setColour (juce::Colour (0xff7d8db8));
        g.drawRoundedRectangle (bounds.toFloat().reduced (1.0f), 8.0f, 1.5f);
    }

    auto inner = bounds.reduced (10);

    // ---- cover area + transport overlay ---------------------------------
    auto cover = inner.removeFromTop (140);
    g.setColour (juce::Colour (0xff14151a));
    g.fillRoundedRectangle (cover.toFloat(), 4.0f);

    juce::Image art;
    if (tf.cachedArtworkPath.existsAsFile())
        art = juce::ImageCache::getFromFile (tf.cachedArtworkPath);

    if (art.isValid())
    {
        g.drawImageWithin (art,
                           cover.getX(), cover.getY(),
                           cover.getWidth(), cover.getHeight(),
                           juce::RectanglePlacement::centred,
                           false);
    }
    else
    {
        g.setColour (juce::Colours::lightgrey.withAlpha (0.5f));
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
        const juce::String placeholder =
            (tf.discogsStatus == TrackFile::DiscogsStatus::Searching)
                ? "(searching…)"
                : ((tf.discogsStatus == TrackFile::DiscogsStatus::NotFound
                  || tf.discogsStatus == TrackFile::DiscogsStatus::Error)
                       ? "(no cover)"
                       : "(no cover yet)");
        g.drawText (placeholder, cover, juce::Justification::centred);
    }

    paintTransportOverlay (g, playHitRect (cover), isPlayingThis);

    inner.removeFromTop (8);

    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, juce::Font::bold)));
    g.drawText  (tf.artist,
                 inner.removeFromTop (16),
                 juce::Justification::centredLeft, true);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));
    g.drawText  (tf.title.isNotEmpty() ? tf.title : tf.filename,
                 inner.removeFromTop (16),
                 juce::Justification::centredLeft, true);

    inner.removeFromTop (4);

    {
        auto row = inner.removeFromTop (14);

        juce::String keyBpm;
        if (tf.key.isNotEmpty()) keyBpm << tf.key;
        if (tf.bpm > 0)
        {
            if (keyBpm.isNotEmpty()) keyBpm << " · ";
            keyBpm << juce::String (tf.bpm) << " BPM";
        }
        g.setColour (juce::Colours::lightgrey);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 10.5f, 0)));
        g.drawText  (keyBpm,
                     row.removeFromLeft (row.getWidth() / 2),
                     juce::Justification::centredLeft);

        g.setColour (juce::Colours::lightgrey.withAlpha (0.7f));
        g.drawText  (TrackFile::discogsStatusText (tf.discogsStatus),
                     row,
                     juce::Justification::centredRight);
    }

    if (! tf.emojiTags.isEmpty())
    {
        g.setColour (juce::Colours::white);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 14.0f, 0)));
        g.drawText  (tf.emojiTags.joinIntoString (" "),
                     inner.removeFromTop (18),
                     juce::Justification::centredLeft);
    }
}

void TrackGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const int n    = numVisible();
    const int cols = columnsFor (getWidth());
    const auto p   = e.getPosition();

    for (int i = 0; i < n; ++i)
    {
        const auto rect = cardRect (i, kCardW, kCardH, cols);
        if (! rect.contains (p))
            continue;

        // Reproduce paintCard's cover-area math so the play-button hit
        // test matches what's drawn.
        auto inner = rect.reduced (10);
        const auto cover   = inner.removeFromTop (140);
        const auto playRect = playHitRect (cover);

        const int libIdx = visibleAt (i);
        if (playRect.contains (p) && libIdx >= 0)
        {
            if (onPlayToggleRequested)
                onPlayToggleRequested (libIdx);
            return;
        }

        // Plain card click -> select
        selectedCardIndex = i;
        repaint();
        if (onTrackSelected) onTrackSelected (libIdx);
        return;
    }

    selectedCardIndex = -1;
    repaint();
    if (onTrackSelected) onTrackSelected (-1);
}
