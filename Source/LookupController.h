#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <vector>

#include "ArtworkCache.h"
#include "DiscogsClient.h"
#include "SpotifyClient.h"
#include "TrackFile.h"

class TrackLibrary;

// Drives artwork lookups for a set of track indices on a single
// background thread, dispatching every state change back onto the JUCE
// message thread.
//
// Source policy (Spotify-first):
//   1. If Spotify credentials are present, try Spotify search.
//      A "good" Spotify match is one that returned a usable album
//      image URL AND we managed to cache the bytes locally.
//   2. If Spotify produced a good match, that wins — Discogs is NOT
//      consulted; artworkSource is set to Spotify.
//   3. Otherwise (no creds / no match / error / no image / cache
//      download failed) we fall back to Discogs.
//   4. Discogs success → artworkSource is Discogs.
//   5. Both fail → artworkSource stays None and the no-cover placeholder
//      is shown.
//
// Threading contract:
//   - startLookup() snapshots the (artist, title) pairs on the message
//     thread so the worker never reads TrackLibrary directly.
//   - All TrackLibrary writes happen via juce::MessageManager::callAsync
//     on the message thread, which keeps tracks[] races out of scope.
//   - The worker sleeps `kInterRequestPauseMs` between requests to be
//     polite to both APIs.
//
// Caching:
//   - Tracks that already have artworkSource != None AND a cached file
//     on disk are skipped on regular lookups. Pass forceRelookup=true
//     to ignore the cache (used by the Re-lookup toolbar button).
class LookupController : public juce::Thread
{
public:
    LookupController (TrackLibrary& library,
                      SpotifyClient& spotify,
                      DiscogsClient& discogs,
                      ArtworkCache&  artwork);
    ~LookupController() override;

    // Summary of what happened on one track. Carried via onTrackProcessed
    // so MainComponent can accumulate counters and the most recent error
    // string for the diagnostics label.
    struct TrackOutcome
    {
        TrackFile::ArtworkSource artworkSource = TrackFile::ArtworkSource::None;
        TrackFile::SpotifyStatus spotifyStatus = TrackFile::SpotifyStatus::NotSearched;
        TrackFile::DiscogsStatus discogsStatus = TrackFile::DiscogsStatus::NotSearched;
        juce::String             spotifyError;
        juce::String             discogsError;
    };

    // (processed, total) — fires on the message thread after each task.
    std::function<void (int processed, int total)> onProgress;

    // Fires on the message thread once each track's result is ready.
    std::function<void (int trackIndex,
                        const TrackOutcome& outcome)> onTrackProcessed;

    // Fires on the message thread when the queue drains (or is cancelled).
    std::function<void()>                          onFinished;

    // Kick off a lookup of the given library indices. Ignored if the
    // worker is already running. The library snapshot happens here
    // (before startThread()) so the worker only sees immutable copies
    // of the artist/title pairs.
    //
    // forceRelookup=true bypasses the "already has artwork" skip — the
    // Re-lookup toolbar button uses this so Dave can refresh artwork
    // from Spotify after first granting credentials.
    void startLookup (const std::vector<int>& libraryIndices,
                      bool forceRelookup = false);

    bool isRunning() const noexcept { return isThreadRunning(); }

    // Cancel the queue. The current in-flight HTTP request will
    // finish; nothing further is processed. Threadsafe.
    void cancel();

private:
    void run() override;

    struct Task
    {
        int          index = -1;
        juce::String artist;
        juce::String title;
    };

    static constexpr int kInterRequestPauseMs = 600; // ~1.6 req/sec

    TrackLibrary&  library;
    SpotifyClient& spotify;
    DiscogsClient& discogs;
    ArtworkCache&  artwork;

    std::vector<Task> tasks;            // worker reads these
    std::atomic<int>  processed { 0 };
    int               total = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookupController)
};
