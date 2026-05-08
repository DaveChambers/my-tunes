#pragma once

#include <juce_graphics/juce_graphics.h>

// On-disk thumbnail cache for cover art fetched from Discogs.
//
// Cached files live under
//   ~/Library/Application Support/My Tunes/ArtworkCache/<sha-of-url>.<ext>
//
// File names are content-addressed (hash of URL) so repeats are free.
// Threading: ensureCachedFile() blocks on a network read when the file
// isn't already cached. Callers are expected to drive it from a worker
// thread.
class ArtworkCache
{
public:
    ArtworkCache();
    ~ArtworkCache();

    // Returns the (possibly empty) file path we'd cache under for `url`.
    // The file might or might not exist yet.
    juce::File getCachedFile (const juce::String& coverUrl) const;

    // Where the cache lives. Created lazily on first download.
    static juce::File getCacheFolder();

    // Synchronous: ensures the cache file for `url` exists locally.
    // - returns the cached file if present (no network),
    // - otherwise downloads from `url`, writes the bytes to disk, and
    //   returns the cached file,
    // - returns juce::File{} on failure.
    juce::File ensureCachedFile (const juce::String& url);

    // Loads the cached image from disk if available. Uses
    // juce::ImageCache so repeat draws reuse the in-memory bitmap.
    juce::Image loadCachedImage (const juce::File& file) const;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtworkCache)
};
