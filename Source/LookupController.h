#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <vector>

#include "ArtworkCache.h"
#include "DiscogsClient.h"
#include "TrackFile.h"

class TrackLibrary;

// Drives Discogs lookups (and the matching cover-art downloads) for a
// set of track indices on a single background thread, dispatching every
// state change back onto the JUCE message thread.
//
// Threading contract:
//   - start() snapshots the (artist, title) pairs on the message thread
//     so the worker never reads from TrackLibrary directly.
//   - All TrackLibrary writes happen via juce::MessageManager::callAsync
//     on the message thread, which keeps tracks[] races out of scope.
//   - The worker sleeps `kInterRequestPauseMs` between requests so we
//     stay well under Discogs' 60-req/min rate limit (auth or not).
//
// Caching:
//   - Tracks already in DiscogsStatus::Found with a cached artwork file
//     are skipped on subsequent lookups (no re-query, no re-download).
class LookupController : public juce::Thread
{
public:
    LookupController (TrackLibrary& library,
                      DiscogsClient& discogs,
                      ArtworkCache&  artwork);
    ~LookupController() override;

    // (processed, total) — fires on the message thread after each task.
    std::function<void (int processed, int total)> onProgress;

    // Fires on the message thread once each track's result is ready.
    // Carries the per-track Discogs result so MainComponent can
    // accumulate a summary (found/uncertain/not found/error counts +
    // most recent error string) for the status label.
    std::function<void (int trackIndex,
                        const DiscogsClient::Result& r)> onTrackProcessed;

    // Fires on the message thread when the queue drains (or is cancelled).
    std::function<void()>                          onFinished;

    // Kick off a lookup of the given library indices. Ignored if the
    // worker is already running. The library snapshot happens here
    // (i.e. before startThread()) so the worker only sees immutable
    // copies of the artist/title pairs.
    void startLookup (const std::vector<int>& libraryIndices);

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
    DiscogsClient& discogs;
    ArtworkCache&  artwork;

    std::vector<Task> tasks;            // worker reads these
    std::atomic<int>  processed { 0 };
    int               total = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookupController)
};
