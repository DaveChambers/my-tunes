#include "TrackLibrary.h"

#include "TrackFilenameParser.h"

TrackLibrary::TrackLibrary()  = default;
TrackLibrary::~TrackLibrary() = default;

void TrackLibrary::scanFolder (const juce::File& f, int sampleSize)
{
    folder = f;
    tracks.clear();

    if (! f.isDirectory())
        return;

    const auto& exts = TrackFilenameParser::supportedExtensions();

    // Walk the folder recursively. Stop once we've hit the sample limit
    // — first-version contract is "at most sampleSize tracks".
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
