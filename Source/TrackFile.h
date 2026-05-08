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
    juce::String  discogsCoverUrl;
    juce::File    cachedArtworkPath;       // if a thumbnail was downloaded
    juce::String  discogsErrorReason;      // populated when status is Error

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
};
