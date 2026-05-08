#include "TrackListComponent.h"

#include "EmojiTagBoardComponent.h"
#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

namespace
{
    // Draws a play or stop glyph centred in `bounds`.
    void paintTransportGlyph (juce::Graphics& g,
                              juce::Rectangle<int> bounds,
                              bool                 isPlaying)
    {
        const auto fb = bounds.toFloat();
        const float cx = fb.getCentreX();
        const float cy = fb.getCentreY();
        const float r  = juce::jmin (fb.getWidth(), fb.getHeight()) * 0.32f;

        if (isPlaying)
        {
            // Stop: square
            const float side = r * 1.5f;
            g.fillRect (juce::Rectangle<float> (cx - side / 2,
                                                 cy - side / 2,
                                                 side, side));
        }
        else
        {
            // Play: right-pointing triangle
            juce::Path p;
            p.startNewSubPath (cx - r * 0.6f, cy - r);
            p.lineTo          (cx - r * 0.6f, cy + r);
            p.lineTo          (cx + r,        cy);
            p.closeSubPath();
            g.fillPath (p);
        }
    }
}

TrackListComponent::TrackListComponent (TrackLibrary& l, TrackPreviewPlayer& p)
    : library (l), player (p), table ("tracks", this)
{
    addAndMakeVisible (table);

    table.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff202229));
    table.setHeaderHeight (24);
    table.setRowHeight (28);
    table.getHeader().setStretchToFitActive (true);

    auto& h = table.getHeader();
    h.addColumn ("Key",       ColKey,       60);
    h.addColumn ("BPM",       ColBpm,       60);
    h.addColumn ("Artist",    ColArtist,   200);
    h.addColumn ("Title",     ColTitle,    260);
    h.addColumn ("Version",   ColVersion,  170);
    h.addColumn ("Tags",      ColTags,     110);
    h.addColumn ("Priority",  ColPriority,  72);
    h.addColumn ("Discogs",   ColDiscogs,   90);
    h.addColumn ("Transport", ColTransport, 70);

    library.addChangeListener (this);
    player .addChangeListener (this);
}

TrackListComponent::~TrackListComponent()
{
    library.removeChangeListener (this);
    player .removeChangeListener (this);
}

void TrackListComponent::resized()
{
    table.setBounds (getLocalBounds());
}

void TrackListComponent::refresh()
{
    table.updateContent();
    table.repaint();
}

void TrackListComponent::setVisibleTrackIndices (std::vector<int> indices)
{
    visibleIndices = std::move (indices);
    table.deselectAllRows();
    refresh();
}

void TrackListComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

int TrackListComponent::rowToLibraryIndex (int rowNumber) const
{
    if (rowNumber < 0) return -1;
    if (visibleIndices.empty())
        return rowNumber < library.getNumTracks() ? rowNumber : -1;
    if (rowNumber < (int) visibleIndices.size())
        return visibleIndices[(std::size_t) rowNumber];
    return -1;
}

int TrackListComponent::getNumRows()
{
    return visibleIndices.empty() ? library.getNumTracks()
                                  : (int) visibleIndices.size();
}

void TrackListComponent::paintRowBackground (juce::Graphics& g, int rowNumber,
                                             int /*width*/, int /*height*/,
                                             bool rowIsSelected)
{
    const int idx          = rowToLibraryIndex (rowNumber);
    const bool isPlayingRow = (idx >= 0 && idx == player.getPlayingTrackIndex());

    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff3a4156));
    else if (isPlayingRow)
        g.fillAll (juce::Colour (0xff2a3140));
    else if (rowNumber % 2 == 0)
        g.fillAll (juce::Colour (0xff23252c));
    else
        g.fillAll (juce::Colour (0xff1f2127));
}

void TrackListComponent::paintCell (juce::Graphics& g, int rowNumber, int columnId,
                                    int width, int height, bool /*rowIsSelected*/)
{
    const int idx = rowToLibraryIndex (rowNumber);
    if (idx < 0 || idx >= library.getNumTracks())
        return;

    const auto& tf = library.getTrack (idx);

    if (columnId == ColTransport)
    {
        const bool isPlayingThis = (player.getPlayingTrackIndex() == idx);
        g.setColour (isPlayingThis ? juce::Colour (0xffd0d8e8)
                                    : juce::Colour (0xffaab0bd));
        paintTransportGlyph (g,
                              juce::Rectangle<int> (0, 0, width, height).reduced (10, 5),
                              isPlayingThis);
        return;
    }

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 13.0f, 0)));

    juce::String text;
    switch (columnId)
    {
        case ColKey:      text = tf.key; break;
        case ColBpm:      text = tf.bpm > 0 ? juce::String (tf.bpm) : juce::String(); break;
        case ColArtist:   text = tf.artist; break;
        case ColTitle:    text = tf.title.isNotEmpty() ? tf.title : tf.filename; break;
        case ColVersion:  text = tf.version; break;
        case ColTags:     text = tf.emojiTags.joinIntoString (" "); break;
        case ColPriority: text = tf.priority > 0 ? juce::String (tf.priority) : juce::String(); break;
        case ColDiscogs:  text = TrackFile::discogsStatusText (tf.discogsStatus); break;
        default:          break;
    }

    g.drawText (text,
                juce::Rectangle<int> (4, 0, width - 8, height),
                juce::Justification::centredLeft,
                /*useEllipsesIfTooBig*/ true);
}

void TrackListComponent::cellClicked (int rowNumber, int columnId,
                                       const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showTagPopupMenu (rowNumber);
        return;
    }

    if (columnId == ColTransport)
    {
        const int idx = rowToLibraryIndex (rowNumber);
        if (idx >= 0 && onPlayToggleRequested)
            onPlayToggleRequested (idx);
    }
}

void TrackListComponent::selectedRowsChanged (int lastRowSelected)
{
    if (onTrackSelected)
        onTrackSelected (rowToLibraryIndex (lastRowSelected));
}

void TrackListComponent::showTagPopupMenu (int rowNumber)
{
    const int trackIdx = rowToLibraryIndex (rowNumber);
    if (trackIdx < 0) return;

    const auto& tf = library.getTrack (trackIdx);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Tag with...");

    const auto& boards = EmojiTagBoardComponent::getBoards();
    for (int i = 0; i < (int) boards.size(); ++i)
    {
        const auto& b      = boards[(std::size_t) i];
        const bool  hasTag = tf.emojiTags.contains (b.emoji);
        menu.addItem (i + 1, b.emoji + "  " + b.label,
                      /*isActive=*/ true, /*isTicked=*/ hasTag);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&table),
                        [this, trackIdx] (int chosen)
    {
        if (chosen <= 0) return;
        const auto& boardsCb = EmojiTagBoardComponent::getBoards();
        const int   boardIdx = chosen - 1;
        if (boardIdx < 0 || boardIdx >= (int) boardsCb.size()) return;
        library.toggleEmojiTag (trackIdx, boardsCb[(std::size_t) boardIdx].emoji);
    });
}
