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

        // Walk recursively. Stop once we've hit the sample limit —
        // first-version contract is "at most sampleSize tracks".
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

            if ((int) tracks.size() >= sampleSize)
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
