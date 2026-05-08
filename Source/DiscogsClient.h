#pragma once

#include <juce_core/juce_core.h>

#include "TrackFile.h"

// Thin synchronous wrapper around the Discogs HTTP API.
//
// Threading: methods here BLOCK on HTTP. Callers run them off the
// message thread. LookupController owns the worker that does this.
//
// Auth: DISCOGS_USER_TOKEN env var when present. Without a token the
// client still works — Discogs throttles harder, but unauthenticated
// search is supported.
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

    struct Result
    {
        TrackFile::DiscogsStatus status = TrackFile::DiscogsStatus::Error;
        juce::String matchedArtist;
        juce::String matchedTitle;
        juce::String releaseId;
        juce::String resourceUrl;
        juce::String coverUrl;
        juce::String thumbUrl;

        // Human-readable diagnostic. Empty on success. Populated for
        // every error path: HTTP non-2xx, JSON parse failure, network
        // timeout, etc. Surfaced in the UI's status label so Dave can
        // see why a lookup pass went wrong.
        juce::String errorReason;
    };

    Result lookup (const juce::String& artist, const juce::String& title);

    static juce::String getUserAgent();

private:
    juce::String token;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscogsClient)
};
