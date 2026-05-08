#include "TrackListComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"

namespace
{
    juce::String discogsStatusText (TrackFile::DiscogsStatus s)
    {
        switch (s)
        {
            case TrackFile::DiscogsStatus::NotSearched: return "—";
            case TrackFile::DiscogsStatus::Searching:   return "searching";
            case TrackFile::DiscogsStatus::Found:       return "found";
            case TrackFile::DiscogsStatus::Uncertain:   return "uncertain";
            case TrackFile::DiscogsStatus::NotFound:    return "not found";
            case TrackFile::DiscogsStatus::Error:       return "error";
        }
        return "—";
    }
}

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
}

TrackListComponent::~TrackListComponent() = default;

void TrackListComponent::resized()
{
    table.setBounds (getLocalBounds());
}

void TrackListComponent::refresh()
{
    table.updateContent();
    table.repaint();
}

int TrackListComponent::getNumRows()
{
    return library.getNumTracks();
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
    if (rowNumber < 0 || rowNumber >= library.getNumTracks())
        return;

    const auto& tf = library.getTrack (rowNumber);

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
        case ColDiscogs:  text = discogsStatusText (tf.discogsStatus); break;
        default:          break;
    }

    g.drawText (text,
                juce::Rectangle<int> (4, 0, width - 8, height),
                juce::Justification::centredLeft,
                /*useEllipsesIfTooBig*/ true);
}
