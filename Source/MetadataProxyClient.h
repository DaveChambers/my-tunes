#pragma once

#include <juce_core/juce_core.h>

// Thin client for the TuneList metadata proxy hosted on Cloud Run.
//
// Endpoint:
//   GET https://tunelist-metadata-96674523622.europe-north1.run.app/lookup
//       ?artist=<artist>&title=<title>
//
// The proxy fronts Spotify and Discogs and keeps the API credentials
// in Google Secret Manager, so the macOS app no longer needs any
// local SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET / DISCOGS_USER_TOKEN.
//
// The proxy already implements the Spotify-first / Discogs-fallback
// policy server-side and returns a single normalised payload telling
// us which source won, plus enough metadata to render the detail
// panel.
//
// Threading: lookup() BLOCKS on HTTP. Run it on a worker thread.
// LookupController is the only owner.
class MetadataProxyClient
{
public:
    explicit MetadataProxyClient (juce::String endpointUrl = getDefaultEndpoint());
    ~MetadataProxyClient();

    // Default endpoint baked into the build, or $TUNELIST_METADATA_URL
    // if set. Overridable in tests. No credentials are required.
    static juce::String getDefaultEndpoint();

    enum class Source
    {
        None,
        Spotify,
        Discogs,
        Error      // backend itself reported {"source":"error", ...}
    };

    struct Result
    {
        Source        source = Source::None;

        // Normalised metadata about the chosen match. May be empty
        // when source == None or Error.
        juce::String  artist;
        juce::String  title;
        juce::String  album;

        // Image to fetch + cache locally. Empty when no artwork.
        juce::String  imageUrl;

        // Spotify-specific fields (present only when source==Spotify
        // OR the proxy chose to surface them anyway).
        juce::String  spotifyTrackId;
        juce::String  spotifyExternalUrl;

        // Discogs-specific fields (present only when source==Discogs
        // OR the proxy chose to surface them anyway).
        juce::String  discogsReleaseId;
        juce::String  discogsExternalUrl;

        // Diagnostic — the actual query string the proxy ran.
        juce::String  query;

        // Populated for every error path so the UI can surface the
        // reason on the diagnostics line. Empty on success.
        juce::String  errorReason;

        // HTTP status code from the proxy. 0 means "no request ever
        // made". 200..299 means success. Anything else is the code we
        // got back — useful for telling auth/quota issues from
        // genuine cache-miss outcomes.
        int           httpCode = 0;
    };

    // Synchronous lookup. URL-encodes artist + title and issues a GET
    // to the configured endpoint. Returns a fully populated Result
    // even on failure (with source=Error + errorReason).
    Result lookup (const juce::String& artist, const juce::String& title);

    // Parse a raw response body into a Result. Static so tests can
    // exercise the JSON branches without hitting the live service.
    // Caller is responsible for filling in artist/title context if
    // it wants them echoed back on errors.
    static Result parseResponseJson (const juce::String& body);

    juce::String getEndpoint() const { return endpoint; }

private:
    juce::String endpoint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetadataProxyClient)
};
