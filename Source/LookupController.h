#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <vector>

#include "ArtworkCache.h"
#include "MetadataProxyClient.h"
#include "TrackFile.h"

class TrackLibrary;

// Drives artwork lookups for a set of track indices on a single
// background thread, dispatching every state change back onto the
// JUCE message thread.
//
// Source policy:
//   The Spotify-first / Discogs-fallback policy now lives server-side
//   in the TuneList Cloud Run proxy (MetadataProxyClient). This class
//   simply calls the proxy once per track, caches whatever image the
//   proxy returns, and writes the result into the TrackLibrary.
//
//   - source=Spotify  -> artworkSource=Spotify, image cached, spotify
//                        fields populated.
//   - source=Discogs  -> artworkSource=Discogs, image cached, discogs
//                        fields populated.
//   - source=None     -> artworkSource=None, no image, both
//                        per-source statuses cleared.
//   - source=Error    -> artworkSource=None, errorReason surfaced on
//                        the diagnostics line for the user.
//
// Threading contract:
//   - startLookup() snapshots the (artist, title) pairs on the message
//     thread so the worker never reads TrackLibrary directly.
//   - All TrackLibrary writes happen via juce::MessageManager::callAsync
//     on the message thread.
//   - The worker sleeps `kInterRequestPauseMs` between requests to
//     stay polite to the proxy.
//
// Caching:
//   - Tracks that already have artworkSource != None AND a cached file
//     on disk are skipped on regular lookups. Pass forceRelookup=true
//     to ignore the cache (Re-lookup toolbar button).
class LookupController : public juce::Thread
{
public:
    LookupController (TrackLibrary&        library,
                      MetadataProxyClient& metadata,
                      ArtworkCache&        artwork);
    ~LookupController() override;

    // Summary of what happened on one track. Carried via onTrackProcessed
    // so MainComponent can accumulate counters and the most recent error
    // string for the diagnostics label.
    struct TrackOutcome
    {
        TrackFile::ArtworkSource artworkSource = TrackFile::ArtworkSource::None;
        MetadataProxyClient::Source proxySource = MetadataProxyClient::Source::None;
        juce::String                proxyError;
        int                         httpCode = 0;
    };

    // (processed, total) — fires on the message thread after each task.
    std::function<void (int processed, int total)> onProgress;

    // Fires on the message thread once each track's result is ready.
    std::function<void (int trackIndex,
                        const TrackOutcome& outcome)> onTrackProcessed;

    // Fires on the message thread when the queue drains (or is cancelled).
    std::function<void()> onFinished;

    // Kick off a lookup of the given library indices. Ignored if the
    // worker is already running. The library snapshot happens here
    // (before startThread()) so the worker only sees immutable copies
    // of the artist/title pairs.
    //
    // forceRelookup=true bypasses the "already has artwork" skip — the
    // Re-lookup toolbar button uses this.
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

    static constexpr int kInterRequestPauseMs = 250; // ~4 req/sec, polite

    TrackLibrary&        library;
    MetadataProxyClient& metadata;
    ArtworkCache&        artwork;

    std::vector<Task> tasks;            // worker reads these
    std::atomic<int>  processed { 0 };
    int               total = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookupController)
};
