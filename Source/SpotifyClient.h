#pragma once

#include <juce_core/juce_core.h>

#include "TrackFile.h"

// Thin synchronous wrapper around the Spotify Web API.
//
// Auth: Client Credentials flow. Reads
//   SPOTIFY_CLIENT_ID
//   SPOTIFY_CLIENT_SECRET
// from the environment. No browser redirect / no user login needed —
// this is a public-catalogue lookup. If either env var is missing the
// client stays in CredentialsMissing state and lookup() returns
// immediately without making any network call (so the app still runs
// happily on machines without Spotify configured).
//
// Threading: lookup() BLOCKS on HTTP (token exchange + search). Callers
// must run it on a worker thread. LookupController is the only owner.
//
// Token lifecycle: the bearer token is fetched lazily on first use and
// reused until expiry; each call refreshes if expired. Token + expiry
// are kept in memory only; nothing is written to disk.
class SpotifyClient
{
public:
    SpotifyClient();
    ~SpotifyClient();

    static juce::String getClientIdFromEnvironment();
    static juce::String getClientSecretFromEnvironment();

    bool hasCredentials() const noexcept
    {
        return clientId.isNotEmpty() && clientSecret.isNotEmpty();
    }

    struct Result
    {
        TrackFile::SpotifyStatus status = TrackFile::SpotifyStatus::NotSearched;

        // Best-match track metadata
        juce::String matchedArtist;
        juce::String matchedTitle;
        juce::String trackId;
        juce::String albumId;
        juce::String externalUrl;    // https://open.spotify.com/track/<id>
        juce::String imageUrl;       // album image, usually the medium size

        // Populated for every error path so the UI can surface the
        // reason on the diagnostics line. Empty on success.
        juce::String errorReason;
    };

    // Synchronous search. Returns:
    //   - status=CredentialsMissing if env vars aren't set;
    //   - status=Error with errorReason on auth / HTTP / JSON failures;
    //   - status=NotFound on a clean search with zero hits;
    //   - status=Found / Uncertain if a track came back. "Found" means
    //     the matched artist string contains the input artist (case-
    //     insensitive); "Uncertain" otherwise.
    //
    //   Note: a Found result without an image still counts as Found,
    //   but LookupController treats missing image as a reason to try
    //   Discogs fallback. Surface that policy in the controller, not
    //   here — this class is just a thin API wrapper.
    Result lookup (const juce::String& artist, const juce::String& title);

private:
    juce::String clientId;
    juce::String clientSecret;

    // In-memory bearer token cache.
    juce::String accessToken;
    juce::int64  tokenExpiresAtMs = 0;

    // Ensures `accessToken` is valid (acquires/refreshes if needed).
    // Returns true on success; on failure populates `outError` and
    // leaves accessToken untouched.
    bool ensureAccessToken (juce::String& outError);

    // POST accounts.spotify.com/api/token with client_credentials grant.
    // Returns true on success and updates accessToken + expiry.
    bool fetchClientCredentialsToken (juce::String& outError);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpotifyClient)
};
