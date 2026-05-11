#pragma once

#include <juce_core/juce_core.h>

// Loader for credentials that TuneList needs to talk to Spotify and
// Discogs. Two sources, in priority order:
//
//   1. ~/Library/Application Support/TuneList/config.json
//        {
//          "spotifyClientId":     "...",
//          "spotifyClientSecret": "...",
//          "discogsUserToken":    "..."
//        }
//   2. Environment variables:
//        SPOTIFY_CLIENT_ID
//        SPOTIFY_CLIENT_SECRET
//        DISCOGS_USER_TOKEN
//
// We prefer the config file because GUI apps launched via Finder /
// Dock don't inherit shell env vars reliably — running
//   open build/.../TuneList.app
// from a terminal that has SPOTIFY_CLIENT_ID exported gets the env
// through, but double-clicking the .app in Finder typically does
// not. The config file is the robust path for normal users; env
// vars stay supported so terminal launches keep working.
//
// SECURITY:
//   - Both sources are read at startup only. Values are kept in
//     memory inside SpotifyClient / DiscogsClient and never written
//     anywhere. Nothing on this class is logged.
//   - config.json is excluded from version control (see .gitignore).
//   - No accessor returns the secret in any form that the UI can
//     display in full.
class TuneListConfig
{
public:
    TuneListConfig();
    ~TuneListConfig();

    // Where the config file is expected. Created lazily if the user
    // writes one; we never auto-create it.
    static juce::File getConfigFile();

    // Effective values (config.json wins; env var used if config
    // key is missing or empty).
    juce::String getSpotifyClientId()     const noexcept { return spotifyClientId; }
    juce::String getSpotifyClientSecret() const noexcept { return spotifyClientSecret; }
    juce::String getDiscogsUserToken()    const noexcept { return discogsUserToken; }

    // Source-of-truth annotation per-field, for the diagnostics line
    // and the no-secret report at startup. Values:
    //   "config.json", "env", or "" (neither found).
    juce::String getSpotifyClientIdSource()     const noexcept { return spotifyClientIdSrc; }
    juce::String getSpotifyClientSecretSource() const noexcept { return spotifyClientSecretSrc; }
    juce::String getDiscogsUserTokenSource()    const noexcept { return discogsUserTokenSrc; }

    bool hasSpotifyCredentials() const noexcept
    {
        return spotifyClientId.isNotEmpty() && spotifyClientSecret.isNotEmpty();
    }

private:
    juce::String spotifyClientId;
    juce::String spotifyClientSecret;
    juce::String discogsUserToken;

    juce::String spotifyClientIdSrc;
    juce::String spotifyClientSecretSrc;
    juce::String discogsUserTokenSrc;

    static juce::String env (const char* name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneListConfig)
};
