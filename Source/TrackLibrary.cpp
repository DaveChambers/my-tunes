#include "TrackLibrary.h"

#include "TrackFilenameParser.h"

TrackLibrary::TrackLibrary()  = default;
TrackLibrary::~TrackLibrary() = default;

void TrackLibrary::scanFolder (const juce::File& f, int sampleSize)
{
    folder = f;
    tracks.clear();

    if (f.isDirectory())
    {
        const auto& exts = TrackFilenameParser::supportedExtensions();

        // Walk recursively. sampleSize > 0 caps the load (mainly for
        // tests); sampleSize <= 0 means "load everything".
        for (const auto& entry : juce::RangedDirectoryIterator (f,
                                                                /*recursive*/ true,
                                                                "*",
                                                                juce::File::findFiles))
        {
            const auto file = entry.getFile();
            const auto ext  = file.getFileExtension().substring (1).toLowerCase();
            if (! exts.contains (ext))
                continue;

            TrackFile tf;
            TrackFilenameParser::parse (tf, file);
            tracks.push_back (std::move (tf));

            if (sampleSize > 0 && (int) tracks.size() >= sampleSize)
                break;
        }
    }

    sendChangeMessage();
}

bool TrackLibrary::toggleEmojiTag (int trackIndex, const juce::String& emoji)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size() || emoji.isEmpty())
        return false;

    auto& tf = tracks[(std::size_t) trackIndex];
    const int existing = tf.emojiTags.indexOf (emoji);
    bool nowHasTag;
    if (existing >= 0)
    {
        tf.emojiTags.remove (existing);
        nowHasTag = false;
    }
    else
    {
        tf.emojiTags.add (emoji);
        nowHasTag = true;
    }
    tf.rebuildSearchKey();  // tag list is part of the search index

    sendChangeMessage();
    return nowHasTag;
}

int TrackLibrary::countTracksWithTag (const juce::String& emoji) const
{
    if (emoji.isEmpty()) return 0;

    int n = 0;
    for (const auto& tf : tracks)
        if (tf.emojiTags.contains (emoji))
            ++n;
    return n;
}

void TrackLibrary::appendTrack (TrackFile track)
{
    tracks.push_back (std::move (track));
    sendChangeMessage();
}

void TrackLibrary::clear (juce::File folderIfKnown)
{
    tracks.clear();
    folder = std::move (folderIfKnown);
    sendChangeMessage();
}

void TrackLibrary::appendBatchSilently (std::vector<TrackFile>&& batch)
{
    if (batch.empty())
        return;
    tracks.reserve (tracks.size() + batch.size());
    for (auto& tf : batch)
        tracks.push_back (std::move (tf));
    // Deliberately NO sendChangeMessage here — the scanner orchestrates
    // a single broadcast at the end of the run.
}
