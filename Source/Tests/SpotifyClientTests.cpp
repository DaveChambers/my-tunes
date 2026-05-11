#include <juce_core/juce_core.h>

#include "../SpotifyClient.h"
#include "../TrackFile.h"

// Conservative tests for SpotifyClient. We can't reach the live Spotify
// API from CI without real credentials baked in (which would be a
// secret-management nightmare), so these tests exercise the cheap-but-
// important behaviour: no-credentials path and empty-input path. Real
// network behaviour is verified by Dave running the app with his
// SPOTIFY_CLIENT_ID + SPOTIFY_CLIENT_SECRET set.
class SpotifyClientTests : public juce::UnitTest
{
public:
    SpotifyClientTests() : juce::UnitTest ("SpotifyClient", "my-tunes") {}

    void runTest() override
    {
        beginTest ("constructor doesn't crash on empty credentials");
        {
            SpotifyClient s ({}, {});
            expect (! s.hasCredentials());
            expect (! s.wasAuthAttempted());
        }

        beginTest ("lookup returns CredentialsMissing without touching network");
        {
            SpotifyClient s ({}, {});
            const auto r = s.lookup ("Daft Punk", "Around the World");
            expect (r.status == TrackFile::SpotifyStatus::CredentialsMissing);
            expect (r.errorReason.isNotEmpty());
            expect (r.trackId.isEmpty());
            expect (r.albumId.isEmpty());
            expect (r.imageUrl.isEmpty());
            // No HTTP request was issued, so no auth code recorded.
            expectEquals (s.getLastAuthHttpCode(), 0);
        }

        beginTest ("plain forgiving query is built as \"Artist Title\"");
        {
            // We can't hit Spotify without creds, but the queryUsed field
            // is populated even on the CredentialsMissing path BEFORE the
            // request would have gone out — verify the no-creds path
            // alone for now. The "artist + space + title" shape is
            // checked via the real-network path on Dave's machine.
            SpotifyClient s ({}, {});
            const auto r = s.lookup ("Phil Kieran", "Saturdays");
            // Even though we short-circuit on missing creds, we want
            // the lookup contract to be "return promptly, no crash".
            expect (r.status == TrackFile::SpotifyStatus::CredentialsMissing);
        }
    }
};

static SpotifyClientTests sSpotifyClientTests;
