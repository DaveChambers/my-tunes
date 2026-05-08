#include "TrackGridComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"

namespace
{
    constexpr int kCardW       = 200;
    constexpr int kCardH       = 240;
    constexpr int kGutter      = 12;
    constexpr int kOuterMargin = 12;

    int columnsFor (int width)
    {
        const int usable = juce::jmax (kCardW, width - 2 * kOuterMargin);
        const int cols   = juce::jmax (1, (usable + kGutter) / (kCardW + kGutter));
        return cols;
    }
}

TrackGridComponent::TrackGridComponent (TrackLibrary& l) : library (l)
{
    library.addChangeListener (this);
}

TrackGridComponent::~TrackGridComponent()
{
    library.removeChangeListener (this);
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

    for (int i = 0; i < n; ++i)
    {
        const int libIdx = visibleAt (i);
        if (libIdx < 0) continue;
        const auto rect = cardRect (i, kCardW, kCardH, cols);
        paintCard (g, libIdx, rect, i == selectedCardIndex);
    }
}

void TrackGridComponent::paintCard (juce::Graphics& g, int libraryIndex,
                                    juce::Rectangle<int> bounds, bool selected) const
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

    // ---- cover area -----------------------------------------------------
    auto cover = inner.removeFromTop (140);
    g.setColour (juce::Colour (0xff14151a));
    g.fillRoundedRectangle (cover.toFloat(), 4.0f);
    g.setColour (juce::Colours::lightgrey.withAlpha (0.5f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
    g.drawText  ("(no cover yet)", cover, juce::Justification::centred);

    inner.removeFromTop (8);

    // ---- artist ---------------------------------------------------------
    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, juce::Font::bold)));
    g.drawText  (tf.artist,
                 inner.removeFromTop (16),
                 juce::Justification::centredLeft,
                 /*useEllipsesIfTooBig=*/ true);

    // ---- title ----------------------------------------------------------
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));
    g.drawText  (tf.title.isNotEmpty() ? tf.title : tf.filename,
                 inner.removeFromTop (16),
                 juce::Justification::centredLeft,
                 /*useEllipsesIfTooBig=*/ true);

    inner.removeFromTop (4);

    // ---- key / BPM + Discogs status ------------------------------------
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

    // ---- emoji tags row -------------------------------------------------
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

    for (int i = 0; i < n; ++i)
    {
        if (cardRect (i, kCardW, kCardH, cols).contains (e.getPosition()))
        {
            selectedCardIndex = i;
            repaint();
            if (onTrackSelected)
                onTrackSelected (visibleAt (i));
            return;
        }
    }
    selectedCardIndex = -1;
    repaint();
    if (onTrackSelected)
        onTrackSelected (-1);
}
