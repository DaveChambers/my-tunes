#include "LookupController.h"

#include "TrackLibrary.h"

LookupController::LookupController (TrackLibrary& l,
                                     MetadataProxyClient& m,
                                     ArtworkCache&        a)
    : juce::Thread ("TuneList-Lookup"),
      library (l), metadata (m), artwork (a)
{
}

LookupController::~LookupController()
{
    cancel();
    stopThread (8000);
}

void LookupController::startLookup (const std::vector<int>& libraryIndices,
                                    bool forceRelookup)
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

        // Cache: skip tracks that already have artwork with a real
        // cached file on disk. Re-lookup bypasses this.
        const bool alreadyResolved =
            tf.artworkSource != TrackFile::ArtworkSource::None
            && tf.cachedArtworkPath.existsAsFile();
        if (alreadyResolved && ! forceRelookup)
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

    juce::Logger::writeToLog (
        "Lookup: starting batch of " + juce::String (total)
        + " track(s) against " + metadata.getEndpoint());

    for (auto& task : tasks)
    {
        if (threadShouldExit())
            break;

        const int idx = task.index;
        juce::Logger::writeToLog (
            "Lookup[" + juce::String (idx) + "]: artist=\""
            + task.artist + "\" title=\"" + task.title + "\"");

        // 1) Mark Searching on the message thread.
        juce::MessageManager::callAsync ([&libraryRef, idx]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);
            // We don't know which source will win until the proxy
            // responds; mark both per-source status fields as
            // Searching so the per-card label shows movement.
            tf.spotifyStatus = TrackFile::SpotifyStatus::Searching;
            tf.discogsStatus = TrackFile::DiscogsStatus::Searching;
            libraryRef.sendChangeMessage();
        });

        // 2) Call the proxy.
        const auto result = metadata.lookup (task.artist, task.title);

        // 3) Cache the artwork if the proxy gave us a usable image.
        juce::File artFile;
        if (result.imageUrl.isNotEmpty())
            artFile = artwork.ensureCachedFile (result.imageUrl);

        const bool artworkCached = artFile.existsAsFile() && artFile.getSize() > 0;

        // 4) Apply on the message thread.
        const auto resultCopy   = result;
        const auto artFileCopy  = artFile;
        const bool artworkCachedCopy = artworkCached;

        juce::MessageManager::callAsync ([&libraryRef, idx, resultCopy,
                                          artFileCopy, artworkCachedCopy]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);

            tf.metadataQuery        = resultCopy.query;
            tf.artworkFallbackReason.clear();

            // Map the proxy's source into TrackFile state.
            using PS = MetadataProxyClient::Source;
            switch (resultCopy.source)
            {
                case PS::Spotify:
                {
                    tf.artworkSource      = artworkCachedCopy
                                              ? TrackFile::ArtworkSource::Spotify
                                              : TrackFile::ArtworkSource::None;
                    tf.cachedArtworkPath  = artworkCachedCopy ? artFileCopy : juce::File();

                    tf.spotifyStatus      = TrackFile::SpotifyStatus::Found;
                    tf.spotifyArtist      = resultCopy.artist;
                    tf.spotifyTitle       = resultCopy.title;
                    tf.spotifyAlbum       = resultCopy.album;
                    tf.spotifyImageUrl    = resultCopy.imageUrl;
                    tf.spotifyTrackId     = resultCopy.spotifyTrackId;
                    tf.spotifyExternalUrl = resultCopy.spotifyExternalUrl;
                    tf.spotifyQuery       = resultCopy.query;
                    tf.spotifyErrorReason.clear();

                    tf.discogsStatus      = TrackFile::DiscogsStatus::NotSearched;
                    tf.discogsErrorReason.clear();

                    if (! artworkCachedCopy)
                        tf.artworkFallbackReason = "Spotify image cache failed";
                    break;
                }
                case PS::Discogs:
                {
                    tf.artworkSource      = artworkCachedCopy
                                              ? TrackFile::ArtworkSource::Discogs
                                              : TrackFile::ArtworkSource::None;
                    tf.cachedArtworkPath  = artworkCachedCopy ? artFileCopy : juce::File();

                    tf.discogsStatus      = TrackFile::DiscogsStatus::Found;
                    tf.discogsArtist      = resultCopy.artist;
                    tf.discogsTitle       = resultCopy.title;
                    tf.discogsReleaseId   = resultCopy.discogsReleaseId;
                    tf.discogsCoverUrl    = resultCopy.imageUrl;
                    tf.discogsExternalUrl = resultCopy.discogsExternalUrl;
                    tf.discogsErrorReason.clear();

                    tf.spotifyStatus      = TrackFile::SpotifyStatus::NotSearched;
                    tf.spotifyErrorReason.clear();

                    if (! artworkCachedCopy)
                        tf.artworkFallbackReason = "Discogs image cache failed";
                    break;
                }
                case PS::None:
                {
                    tf.artworkSource      = TrackFile::ArtworkSource::None;
                    tf.cachedArtworkPath  = juce::File();
                    tf.spotifyStatus      = TrackFile::SpotifyStatus::NotFound;
                    tf.discogsStatus      = TrackFile::DiscogsStatus::NotFound;
                    tf.spotifyErrorReason.clear();
                    tf.discogsErrorReason.clear();
                    tf.artworkFallbackReason = "no match from metadata service";
                    break;
                }
                case PS::Error:
                {
                    tf.artworkSource      = TrackFile::ArtworkSource::None;
                    tf.cachedArtworkPath  = juce::File();
                    tf.spotifyStatus      = TrackFile::SpotifyStatus::Error;
                    tf.discogsStatus      = TrackFile::DiscogsStatus::Error;
                    tf.spotifyErrorReason = resultCopy.errorReason;
                    tf.discogsErrorReason = resultCopy.errorReason;
                    tf.artworkFallbackReason = resultCopy.errorReason;
                    break;
                }
            }

            libraryRef.sendChangeMessage();
        });

        // 4b) Per-track callback for the diagnostics counter.
        if (onTrackProcessed)
        {
            TrackOutcome outcome;
            outcome.proxySource = result.source;
            outcome.proxyError  = result.errorReason;
            outcome.httpCode    = result.httpCode;

            if (result.source == MetadataProxyClient::Source::Spotify && artworkCached)
                outcome.artworkSource = TrackFile::ArtworkSource::Spotify;
            else if (result.source == MetadataProxyClient::Source::Discogs && artworkCached)
                outcome.artworkSource = TrackFile::ArtworkSource::Discogs;
            else
                outcome.artworkSource = TrackFile::ArtworkSource::None;

            const auto cb       = onTrackProcessed;
            const int  trackIdx = idx;
            juce::MessageManager::callAsync ([cb, trackIdx, outcome]
            {
                if (cb) cb (trackIdx, outcome);
            });
        }

        juce::Logger::writeToLog (
            "Lookup[" + juce::String (idx) + "]: final artworkSource="
            + (artworkCached
                ? (result.source == MetadataProxyClient::Source::Spotify
                    ? juce::String ("Spotify")
                    : juce::String ("Discogs"))
                : juce::String ("None")));

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

        // 6) Be polite to the proxy.
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
