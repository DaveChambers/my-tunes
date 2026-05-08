#pragma once

#include <juce_core/juce_core.h>

#include "TrackFile.h"

// Thin synchronous wrapper around the Discogs HTTP API.
//
// Threading note: methods here BLOCK on HTTP. Callers must run them off
// the message thread. LookupController owns the worker thread that
// drives this for the 20-track sample.
//
// Auth:
//   - DISCOGS_USER_TOKEN env var (never committed). If absent the
//     client still works but is more aggressively rate-limited by
//     Discogs.
//
// Per Discogs ToS the User-Agent identifies the app as
//     MyTunes/0.1 +https://makeelectronicsound.com
class DiscogsClient
{
public:
    DiscogsClient();
    ~DiscogsClient();

    static juce::String getTokenFromEnvironment();
    bool hasToken() const noexcept { return token.isNotEmpty(); }

    // Fields the engine reads back from a search.
    struct Result
    {
        TrackFile::DiscogsStatus status = TrackFile::DiscogsStatus::Error;
        juce::String matchedArtist;     // mirrors the input artist
        juce::String matchedTitle;      // Discogs combined "Artist - Album" string
        juce::String releaseId;
        juce::String resourceUrl;       // api.discogs.com/releases/<id>
        juce::String coverUrl;          // larger image
        juce::String thumbUrl;          // thumbnail (small)
    };

    // Synchronous search: returns the best-effort first result for
    // (artist, title). May block for several seconds on a slow network.
    Result lookup (const juce::String& artist, const juce::String& title);

    // User-Agent string used on every outbound request. Exposed for
    // ArtworkCache so the image download carries the same identifier.
    static juce::String getUserAgent();

private:
    juce::String token;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscogsClient)
};
