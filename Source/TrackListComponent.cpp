#include "TrackListComponent.h"

#include <algorithm>

#include "EmojiTagBoardComponent.h"
#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

namespace
{
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
            const float side = r * 1.5f;
            g.fillRect (juce::Rectangle<float> (cx - side / 2,
                                                 cy - side / 2,
                                                 side, side));
        }
        else
        {
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
    // sortable is in defaultFlags so all of these are clickable headers.
    h.addColumn ("Key",       ColKey,       60);
    h.addColumn ("BPM",       ColBpm,       60);
    h.addColumn ("Artist",    ColArtist,   200);
    h.addColumn ("Title",     ColTitle,    260);
    h.addColumn ("Version",   ColVersion,  170);
    h.addColumn ("Tags",      ColTags,     110);
    h.addColumn ("Priority",  ColPriority,  72);
    h.addColumn ("Discogs",   ColDiscogs,   90);
    h.addColumn ("Transport", ColTransport, 70,
                 /*minimumWidth*/ 50, /*maximumWidth*/ -1,
                 /*propertyFlags*/ juce::TableHeaderComponent::visible
                                  | juce::TableHeaderComponent::resizable);

    library.addChangeListener (this);
    player .addChangeListener (this);

    rebuildDisplayedIndices();
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
    rebuildDisplayedIndices();
    table.updateContent();
    table.repaint();
}

void TrackListComponent::setVisibleTrackIndices (std::vector<int> indices)
{
    // Capture the currently-selected LIBRARY index so we can re-anchor
    // the row selection after the filter rebuild. Without this we used
    // to call table.deselectAllRows() which propagates
    // selectedRowsChanged(-1) into MainComponent and wipes the user's
    // selection mid-click whenever a library broadcast lands (e.g. a
    // per-track artwork-lookup result during an 8 000-track session).
    const int prevSelectedLibIdx = rowToLibraryIndex (table.getSelectedRow());

    baseIndices = std::move (indices);
    rebuildDisplayedIndices();
    table.updateContent();

    juce::SparseSet<int> newSelection;
    if (prevSelectedLibIdx >= 0)
    {
        const int newRow = libraryIndexToRow (prevSelectedLibIdx);
        if (newRow >= 0)
            newSelection.addRange ({ newRow, newRow + 1 });
    }
    // dontSendNotification: do NOT forward this filter-driven
    // selection change as a click-style selectedRowsChanged callback.
    // Only the user actually clicking a row should fire that.
    table.setSelectedRows (newSelection, juce::dontSendNotification);

    table.repaint();
}

void TrackListComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Library or player broadcast — track count may have changed (after
    // a scan) or the per-row content may have updated (Discogs / tags /
    // playback state). Rebuild displayed indices so the table mapping
    // is correct, then repaint.
    rebuildDisplayedIndices();
    table.updateContent();
    table.repaint();
}

void TrackListComponent::rebuildDisplayedIndices()
{
    displayedIndices.clear();

    if (baseIndices.empty())
    {
        const int n = library.getNumTracks();
        displayedIndices.reserve ((std::size_t) n);
        for (int i = 0; i < n; ++i)
            displayedIndices.push_back (i);
    }
    else
    {
        displayedIndices = baseIndices;
    }

    if (sortColumn != 0)
        applySortToDisplayedIndices();
}

void TrackListComponent::applySortToDisplayedIndices()
{
    auto& lib = library;
    const int  col = sortColumn;
    const bool fwd = sortForwards;

    auto less = [&lib, col] (int a, int b) -> bool
    {
        const auto& ta = lib.getTrack (a);
        const auto& tb = lib.getTrack (b);
        switch (col)
        {
            case ColKey:      return ta.key.compareNatural (tb.key)         < 0;
            case ColBpm:      return ta.bpm                                  < tb.bpm;
            case ColArtist:   return ta.artist.compareNatural (tb.artist)    < 0;
            case ColTitle:    return ta.title.compareNatural (tb.title)      < 0;
            case ColVersion:  return ta.version.compareNatural (tb.version)  < 0;
            case ColTags:     return ta.emojiTags.joinIntoString (" ")
                                       .compareNatural (tb.emojiTags.joinIntoString (" ")) < 0;
            case ColPriority: return ta.priority                              < tb.priority;
            case ColDiscogs:  return TrackFile::discogsStatusText (ta.discogsStatus)
                                       .compareNatural (TrackFile::discogsStatusText (tb.discogsStatus)) < 0;
            default:          return false;
        }
    };

    auto comp = [less, fwd] (int a, int b) -> bool
    {
        return fwd ? less (a, b) : less (b, a);
    };

    // Stable sort so equal-keyed rows keep their relative order.
    std::stable_sort (displayedIndices.begin(), displayedIndices.end(), comp);
}

int TrackListComponent::rowToLibraryIndex (int rowNumber) const
{
    if (rowNumber < 0) return -1;
    if (rowNumber >= (int) displayedIndices.size()) return -1;
    return displayedIndices[(std::size_t) rowNumber];
}

int TrackListComponent::libraryIndexToRow (int libraryIdx) const
{
    if (libraryIdx < 0) return -1;
    for (int r = 0; r < (int) displayedIndices.size(); ++r)
        if (displayedIndices[(std::size_t) r] == libraryIdx)
            return r;
    return -1;
}

int TrackListComponent::getNumRows()
{
    return (int) displayedIndices.size();
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
        const int idx = rowToLibraryIndex (rowNumber);
        if (idx >= 0)
            showTagPopupMenu (idx, e.getScreenPosition());
        return;
    }

    if (columnId == ColTransport)
    {
        const int idx = rowToLibraryIndex (rowNumber);
        if (idx >= 0 && onPlayToggleRequested)
            onPlayToggleRequested (idx);
    }
}

void TrackListComponent::cellDoubleClicked (int rowNumber, int /*columnId*/,
                                             const juce::MouseEvent& /*e*/)
{
    const int idx = rowToLibraryIndex (rowNumber);
    if (idx >= 0 && onPlayToggleRequested)
        onPlayToggleRequested (idx);
}

void TrackListComponent::selectedRowsChanged (int lastRowSelected)
{
    if (onTrackSelected)
        onTrackSelected (rowToLibraryIndex (lastRowSelected));
}

void TrackListComponent::sortOrderChanged (int newSortColumnId, bool isForwards)
{
    // Preserve selection by library index across the resort.
    const int prevSelectedRow = table.getSelectedRow();
    const int prevLibIdx      = rowToLibraryIndex (prevSelectedRow);

    sortColumn   = newSortColumnId;
    sortForwards = isForwards;
    rebuildDisplayedIndices();

    table.updateContent();
    table.repaint();

    if (prevLibIdx >= 0)
    {
        const int newRow = libraryIndexToRow (prevLibIdx);
        if (newRow >= 0)
            table.selectRow (newRow);
    }
}

void TrackListComponent::showTagPopupMenu (int trackIndex,
                                           juce::Point<int> screenPos)
{
    if (trackIndex < 0 || trackIndex >= library.getNumTracks())
        return;

    const auto& tf = library.getTrack (trackIndex);

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

    auto options = juce::PopupMenu::Options()
                       .withTargetScreenArea (juce::Rectangle<int> (screenPos.x,
                                                                     screenPos.y,
                                                                     1, 1));

    menu.showMenuAsync (options,
                        [this, trackIndex] (int chosen)
    {
        if (chosen <= 0) return;
        const auto& boardsCb = EmojiTagBoardComponent::getBoards();
        const int   boardIdx = chosen - 1;
        if (boardIdx < 0 || boardIdx >= (int) boardsCb.size()) return;
        library.toggleEmojiTag (trackIndex, boardsCb[(std::size_t) boardIdx].emoji);
    });
}
