#include "LookupController.h"

#include "TrackLibrary.h"

LookupController::LookupController (TrackLibrary& l,
                                     DiscogsClient& d,
                                     ArtworkCache& a)
    : juce::Thread ("MyTunes-Lookup"),
      library (l), discogs (d), artwork (a)
{
}

LookupController::~LookupController()
{
    cancel();
    stopThread (8000);
}

void LookupController::startLookup (const std::vector<int>& libraryIndices)
{
    if (isThreadRunning())
        return;

    // Snapshot artist/title on the message thread (this method is
    // called from MainComponent in response to a button click).
    tasks.clear();
    tasks.reserve (libraryIndices.size());

    for (int idx : libraryIndices)
    {
        if (idx < 0 || idx >= library.getNumTracks())
            continue;

        const auto& tf = library.getTrack (idx);

        // Cache: skip tracks we've already matched and have cached art for.
        const bool alreadyResolved =
            tf.discogsStatus == TrackFile::DiscogsStatus::Found
            && tf.cachedArtworkPath.existsAsFile();
        if (alreadyResolved)
            continue;

        Task t;
        t.index  = idx;
        t.artist = tf.artist;
        t.title  = tf.title;
        tasks.push_back (std::move (t));
    }

    total     = (int) tasks.size();
    processed = 0;

    if (total == 0)
    {
        if (onFinished) onFinished();
        return;
    }

    startThread();
}

void LookupController::cancel()
{
    signalThreadShouldExit();
}

void LookupController::run()
{
    auto& libraryRef = library;

    for (auto& task : tasks)
    {
        if (threadShouldExit())
            break;

        // 1) Mark Searching on the message thread.
        const int idx = task.index;
        juce::MessageManager::callAsync ([&libraryRef, idx]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);
            tf.discogsStatus = TrackFile::DiscogsStatus::Searching;
            libraryRef.sendChangeMessage();
        });

        // 2) Synchronous lookup on the worker thread.
        const auto result = discogs.lookup (task.artist, task.title);

        // 3) If we have an image URL, fetch the thumbnail synchronously.
        juce::File artworkPath;
        const auto imgUrl = result.thumbUrl.isNotEmpty() ? result.thumbUrl
                                                          : result.coverUrl;
        if ((result.status == TrackFile::DiscogsStatus::Found
          || result.status == TrackFile::DiscogsStatus::Uncertain)
            && imgUrl.isNotEmpty())
        {
            artworkPath = artwork.ensureCachedFile (imgUrl);
        }

        // 4) Apply on the message thread.
        const auto applied = result;
        const auto path    = artworkPath;
        juce::MessageManager::callAsync ([&libraryRef, idx, applied, path]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);
            tf.discogsStatus     = applied.status;
            tf.discogsArtist     = applied.matchedArtist;
            tf.discogsTitle      = applied.matchedTitle;
            tf.discogsReleaseId  = applied.releaseId;
            tf.discogsCoverUrl   = applied.coverUrl;
            tf.cachedArtworkPath = path;
            libraryRef.sendChangeMessage();
        });

        // 5) Progress.
        const int done = ++processed;
        const int t    = total;
        if (onProgress)
        {
            juce::MessageManager::callAsync ([this, done, t]
            {
                if (onProgress) onProgress (done, t);
            });
        }

        // 6) Be nice to Discogs.
        wait (kInterRequestPauseMs);
    }

    if (onFinished)
    {
        juce::MessageManager::callAsync ([this]
        {
            if (onFinished) onFinished();
        });
    }
}
