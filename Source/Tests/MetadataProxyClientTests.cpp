#include <juce_core/juce_core.h>

#include "../MetadataProxyClient.h"

// MetadataProxyClient tests. We don't hit the live Cloud Run service
// from CI (don't want every test run to burn proxy quota or tie the
// build to network connectivity). The tests cover:
//   - the default endpoint URL,
//   - construction with a custom endpoint,
//   - the empty-input short-circuit (no network call),
//   - the JSON parse branches via parseResponseJson(), which is a
//     static so the parser path is reachable without a real HTTP
//     round-trip.
class MetadataProxyClientTests : public juce::UnitTest
{
public:
    MetadataProxyClientTests() : juce::UnitTest ("MetadataProxyClient", "my-tunes") {}

    void runTest() override
    {
        beginTest ("default endpoint is the Cloud Run /lookup URL");
        {
            const auto ep = MetadataProxyClient::getDefaultEndpoint();
            expect (ep.startsWithIgnoreCase ("https://"));
            expect (ep.contains ("tunelist-metadata"));
            expect (ep.endsWith ("/lookup"));
        }

        beginTest ("constructor doesn't crash with the default endpoint");
        {
            MetadataProxyClient c;
            expectEquals (c.getEndpoint(),
                          MetadataProxyClient::getDefaultEndpoint());
        }

        beginTest ("empty artist + title short-circuits without network");
        {
            MetadataProxyClient c ("https://invalid.example/lookup");
            const auto r = c.lookup ({}, {});
            expect (r.source == MetadataProxyClient::Source::None);
            expect (r.errorReason.isNotEmpty());
            expectEquals (r.httpCode, 0);
        }

        beginTest ("parses source=spotify payload");
        {
            const auto r = MetadataProxyClient::parseResponseJson (R"({
                "source":         "spotify",
                "artist":         "Phil Kieran",
                "title":          "Saturdays",
                "album":          "Night EP",
                "imageUrl":       "https://i.scdn.co/image/abc",
                "spotifyUrl":     "https://open.spotify.com/track/abc",
                "spotifyTrackId": "abc123",
                "query":          "Phil Kieran Saturdays"
            })");

            expect       (r.source == MetadataProxyClient::Source::Spotify);
            expectEquals (r.artist,             juce::String ("Phil Kieran"));
            expectEquals (r.title,              juce::String ("Saturdays"));
            expectEquals (r.album,              juce::String ("Night EP"));
            expectEquals (r.imageUrl,           juce::String ("https://i.scdn.co/image/abc"));
            expectEquals (r.spotifyExternalUrl, juce::String ("https://open.spotify.com/track/abc"));
            expectEquals (r.spotifyTrackId,     juce::String ("abc123"));
            expectEquals (r.query,              juce::String ("Phil Kieran Saturdays"));
            expect       (r.errorReason.isEmpty());
        }

        beginTest ("parses source=discogs payload");
        {
            const auto r = MetadataProxyClient::parseResponseJson (R"({
                "source":     "discogs",
                "artist":     "Obscure Artist",
                "title":      "Obscure Track",
                "album":      "Some Release",
                "imageUrl":   "https://i.discogs.com/abc.jpg",
                "discogsUrl": "https://www.discogs.com/release/12345",
                "discogsId":  "12345",
                "query":      "Obscure Artist Obscure Track"
            })");

            expect       (r.source == MetadataProxyClient::Source::Discogs);
            expectEquals (r.imageUrl,           juce::String ("https://i.discogs.com/abc.jpg"));
            expectEquals (r.discogsExternalUrl, juce::String ("https://www.discogs.com/release/12345"));
            expectEquals (r.discogsReleaseId,   juce::String ("12345"));
            expect       (r.spotifyExternalUrl.isEmpty());
        }

        beginTest ("parses source=none payload");
        {
            const auto r = MetadataProxyClient::parseResponseJson (R"({
                "source":   "none",
                "artist":   "Nobody",
                "title":    "Nothing",
                "imageUrl": null
            })");

            expect (r.source == MetadataProxyClient::Source::None);
            expect (r.imageUrl.isEmpty());
        }

        beginTest ("parses source=error payload and surfaces error");
        {
            const auto r = MetadataProxyClient::parseResponseJson (R"({
                "source": "error",
                "error":  "upstream Spotify auth failed"
            })");

            expect       (r.source == MetadataProxyClient::Source::Error);
            expectEquals (r.errorReason, juce::String ("upstream Spotify auth failed"));
        }

        beginTest ("malformed JSON is reported as Error rather than crashing");
        {
            const auto r = MetadataProxyClient::parseResponseJson ("{ not valid json");
            expect (r.source == MetadataProxyClient::Source::Error);
            expect (r.errorReason.isNotEmpty());
        }

        beginTest ("unknown source value defaults to None");
        {
            const auto r = MetadataProxyClient::parseResponseJson (R"({
                "source": "from-a-future-proxy-version"
            })");
            expect (r.source == MetadataProxyClient::Source::None);
        }
    }
};

static MetadataProxyClientTests sMetadataProxyClientTests;
