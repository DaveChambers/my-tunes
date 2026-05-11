#pragma once

#include <juce_core/juce_core.h>

// One track from a scanned folder. Filename-derived metadata only —
// network-derived metadata (Discogs id, cover URL, …) and user-applied
// metadata (emoji tags, priority, notes) live in dedicated optional
// fields so first-version code paths can be filename-only.
//
// Kept as a plain struct on purpose. UI/list/storage all read it
// directly; no getters/setters needed at this stage.
struct TrackFile
{
    // --- file location -----------------------------------------------------
    juce::File   file;
    juce::String filename;          // file name with extension
    juce::String extension;         // lowercase, no dot — "mp3", "wav", …

    // --- parsed from the filename -----------------------------------------
    juce::String key;               // e.g. "04A"
    int          bpm        = 0;    // 0 == unknown
    juce::String artist;
    juce::String title;
    juce::String version;           // remix / version, may be empty
    bool         filenameParseOk = false;

    // --- user state (filled by tagging UI) --------------------------------
    juce::StringArray emojiTags;    // e.g. { "🌅", "⭐" }
    int               priority = 0; // 0 = unset, 1..5 = user choice
    juce::String      notes;

    // --- Discogs lookup state ---------------------------------------------
    enum class DiscogsStatus
    {
        NotSearched,
        Searching,
        Found,
        Uncertain,
        NotFound,
        Error
    };

    DiscogsStatus discogsStatus = DiscogsStatus::NotSearched;
    juce::String  discogsReleaseId;
    juce::String  discogsArtist;
    juce::String  discogsTitle;
    juce::String  discogsCoverUrl;        // album art URL
    juce::String  discogsExternalUrl;     // www.discogs.com/release/<id>
    juce::String  discogsErrorReason;      // populated when status is Error

    // --- Spotify lookup state ---------------------------------------------
    // Spotify is preferred for artwork; Discogs is the fallback. The
    // status enum mirrors DiscogsStatus so the UI rendering code can
    // treat both uniformly. CredentialsMissing is a separate, non-Error
    // value so the diagnostic line can hint "set SPOTIFY_CLIENT_ID..."
    // without colouring it red.
    enum class SpotifyStatus
    {
        NotSearched,
        Searching,
        Found,
        Uncertain,
        NotFound,
        CredentialsMissing,
        Error
    };

    SpotifyStatus spotifyStatus = SpotifyStatus::NotSearched;
    juce::String  spotifyTrackId;
    juce::String  spotifyAlbumId;
    juce::String  spotifyExternalUrl;     // open.spotify.com/track/<id>
    juce::String  spotifyImageUrl;         // album image we picked
    juce::String  spotifyArtist;
    juce::String  spotifyTitle;
    juce::String  spotifyAlbum;            // album name (e.g. "Untitled EP")
    juce::String  spotifyQuery;            // exact q= we sent to /v1/search
    juce::String  spotifyErrorReason;

    // Diagnostic: when artworkSource ends up Discogs or None, this
    // explains why we didn't use Spotify. Empty string when Spotify
    // won. Surfaced in the detail panel only — purely for Dave's eyes,
    // not used by any control flow.
    juce::String  artworkFallbackReason;

    // The exact query string the metadata proxy ran (artist + title).
    // Surfaced in the detail panel as "Lookup query" so Dave can see
    // exactly what was searched for.
    juce::String  metadataQuery;

    // Match confidence assigned by the desktop client's local scoring
    // pass — "High" / "Medium" / "Rejected" / empty. Rejected means
    // the proxy returned a result but its artist/title was too
    // unrelated to the cleaned filename input, so the desktop refused
    // to paint the artwork. Empty string until a lookup has run.
    juce::String  matchConfidence;

    // --- Which source produced the cover that's currently cached -----------
    // Set when LookupController commits a successful lookup. If a Spotify
    // match has a usable image, artworkSource is Spotify and we do NOT
    // fall through to Discogs. If Spotify has no match/no image/no creds,
    // Discogs is tried; on success artworkSource is Discogs. Otherwise
    // None and the no-cover placeholder is shown.
    enum class ArtworkSource
    {
        None,
        Spotify,
        Discogs
    };

    ArtworkSource artworkSource = ArtworkSource::None;
    juce::File    cachedArtworkPath;       // owned by whichever source won

    // Plain ASCII status text for the table column / detail panel. We
    // deliberately avoid the em-dash / "smart quote" range — earlier
    // builds used "—" as a placeholder and Dave saw that show up as
    // "â€"" mojibake in the column due to UTF-8 being interpreted as
    // Latin-1 by the renderer in some setups.
    static juce::String discogsStatusText (DiscogsStatus s) noexcept
    {
        switch (s)
        {
            case DiscogsStatus::NotSearched: return "Not searched";
            case DiscogsStatus::Searching:   return "Searching";
            case DiscogsStatus::Found:       return "Found";
            case DiscogsStatus::Uncertain:   return "Uncertain";
            case DiscogsStatus::NotFound:    return "Not found";
            case DiscogsStatus::Error:       return "Error";
        }
        return "Not searched";
    }

    static juce::String spotifyStatusText (SpotifyStatus s) noexcept
    {
        switch (s)
        {
            case SpotifyStatus::NotSearched:        return "Not searched";
            case SpotifyStatus::Searching:          return "Searching";
            case SpotifyStatus::Found:              return "Found";
            case SpotifyStatus::Uncertain:          return "Uncertain";
            case SpotifyStatus::NotFound:           return "Not found";
            case SpotifyStatus::CredentialsMissing: return "No credentials";
            case SpotifyStatus::Error:              return "Error";
        }
        return "Not searched";
    }

    static juce::String artworkSourceText (ArtworkSource s) noexcept
    {
        switch (s)
        {
            case ArtworkSource::None:    return "None";
            case ArtworkSource::Spotify: return "Spotify";
            case ArtworkSource::Discogs: return "Discogs";
        }
        return "None";
    }
};
