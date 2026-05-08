#include "TrackListComponent.h"

#include "EmojiTagBoardComponent.h"
#include "TrackFile.h"
#include "TrackLibrary.h"

TrackListComponent::TrackListComponent (TrackLibrary& l)
    : library (l), table ("tracks", this)
{
    addAndMakeVisible (table);

    table.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff202229));
    table.setHeaderHeight (24);
    table.setRowHeight (28);
    table.getHeader().setStretchToFitActive (true);

    auto& h = table.getHeader();
    h.addColumn ("Key",      ColKey,      60);
    h.addColumn ("BPM",      ColBpm,      60);
    h.addColumn ("Artist",   ColArtist,  220);
    h.addColumn ("Title",    ColTitle,   280);
    h.addColumn ("Version",  ColVersion, 180);
    h.addColumn ("Tags",     ColTags,    120);
    h.addColumn ("Priority", ColPriority, 80);
    h.addColumn ("Discogs",  ColDiscogs, 100);

    library.addChangeListener (this);
}

TrackListComponent::~TrackListComponent()
{
    library.removeChangeListener (this);
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
    {
        return rowNumber < library.getNumTracks() ? rowNumber : -1;
    }
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
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff3a4156));
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

void TrackListComponent::cellClicked (int rowNumber, int /*columnId*/,
                                      const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showTagPopupMenu (rowNumber);
        return;
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
        menu.addItem (i + 1,                                   // itemID is 1-based
                      b.emoji + "  " + b.label,
                      /*isActive=*/ true,
                      /*isTicked=*/ hasTag);
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
