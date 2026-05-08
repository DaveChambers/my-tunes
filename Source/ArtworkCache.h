#pragma once

#include <juce_graphics/juce_graphics.h>

// On-disk thumbnail cache for cover art fetched from Discogs. Stubbed
// for the skeleton commit. Cached files live under
//   ~/Library/Application Support/My Tunes/ArtworkCache/
class ArtworkCache
{
public:
    ArtworkCache();
    ~ArtworkCache();

    // Returns the file we'd store the thumbnail for `coverUrl` under.
    // The file might or might not exist yet.
    juce::File getCachedFile (const juce::String& coverUrl) const;

    // Looks up a previously-cached image. Returns an invalid Image if
    // not present.
    juce::Image getCachedImage (const juce::String& coverUrl) const;

    // Where the cache lives. Created lazily on first write.
    static juce::File getCacheFolder();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtworkCache)
};
