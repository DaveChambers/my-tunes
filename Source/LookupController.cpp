#include "LookupController.h"

#include "LookupMatching.h"
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

bool LookupController::shouldEnqueueForLookup (const TrackFile& tf,
                                                bool forceRelookup)
{
    if (forceRelookup)
        return true;
    const bool alreadyResolved =
        tf.artworkSource != TrackFile::ArtworkSource::None
        && tf.cachedArtworkPath.existsAsFile();
    return ! alreadyResolved;
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
        if (! shouldEnqueueForLookup (tf, forceRelookup))
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

        // Clean the parsed filename fields BEFORE asking the proxy. The
        // backend will happily return a "best" Spotify hit for any
        // string we send it (including "You Girls.wav~~"), so removing
        // file-extension + trailing-junk noise is purely about not
        // poisoning Spotify's relevance ranking with garbage.
        const auto cleanedArtist = LookupMatching::cleanForLookup (task.artist);
        const auto cleanedTitle  = LookupMatching::cleanForLookup (task.title);

        juce::Logger::writeToLog (
            "Lookup[" + juce::String (idx) + "]: artist=\""
            + task.artist + "\" title=\"" + task.title + "\""
            + ((cleanedArtist != task.artist || cleanedTitle != task.title)
                ? juce::String (" -> cleaned=\"") + cleanedArtist
                    + "\" / \"" + cleanedTitle + "\""
                : juce::String()));

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

        // 2) Call the proxy with the cleaned strings.
        const auto result = metadata.lookup (cleanedArtist, cleanedTitle);

        // 2b) Score the returned artist/title against the cleaned
        //     input so we can reject obvious mismatches BEFORE caching
        //     any artwork. The proxy's "best" hit is sometimes nonsense
        //     for obscure tracks (Nush "You Girls", Phonat And Super
        //     Mal "Pixelated") and we'd rather show no art than wrong
        //     art for them.
        const auto score = LookupMatching::scoreMatch (
                              cleanedArtist, cleanedTitle,
                              result.artist, result.title);

        const bool proxyOk = (result.source == MetadataProxyClient::Source::Spotify
                            || result.source == MetadataProxyClient::Source::Discogs);
        const bool scoreAccepted = (score.confidence != LookupMatching::Confidence::Rejected);
        const bool accept = proxyOk && scoreAccepted;

        juce::Logger::writeToLog (
            juce::String ("Lookup[") + juce::String (idx) + "]: scoring "
            + "input=\"" + cleanedArtist + " / " + cleanedTitle
            + "\" vs returned=\"" + result.artist + " / " + result.title
            + "\" -> " + LookupMatching::toString (score.confidence)
            + (score.reason.isNotEmpty() ? juce::String (" (")
                                             + score.reason + ")"
                                          : juce::String()));

        // 3) Cache the artwork only if the proxy supplied a usable
        //    image AND the local scoring accepted the match.
        juce::File artFile;
        if (accept && result.imageUrl.isNotEmpty())
            artFile = artwork.ensureCachedFile (result.imageUrl);

        const bool artworkCached = artFile.existsAsFile() && artFile.getSize() > 0;

        // 4) Apply on the message thread.
        const auto resultCopy   = result;
        const auto artFileCopy  = artFile;
        const bool artworkCachedCopy = artworkCached;
        const bool acceptedCopy = accept;
        const juce::String scoreLabel  = LookupMatching::toString (score.confidence);
        const juce::String scoreReason = score.reason;

        juce::MessageManager::callAsync ([&libraryRef, idx, resultCopy,
                                          artFileCopy, artworkCachedCopy,
                                          acceptedCopy, scoreLabel, scoreReason]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);

            tf.metadataQuery        = resultCopy.query;
            tf.matchConfidence      = scoreLabel;
            tf.artworkFallbackReason.clear();

            // Map the proxy's source into TrackFile state.
            using PS = MetadataProxyClient::Source;
            switch (resultCopy.source)
            {
                case PS::Spotify:
                {
                    const bool useArt = acceptedCopy && artworkCachedCopy;
                    tf.artworkSource      = useArt ? TrackFile::ArtworkSource::Spotify
                                                    : TrackFile::ArtworkSource::None;
                    tf.cachedArtworkPath  = useArt ? artFileCopy : juce::File();

                    tf.spotifyStatus      = acceptedCopy
                                              ? TrackFile::SpotifyStatus::Found
                                              : TrackFile::SpotifyStatus::Uncertain;
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

                    if (! acceptedCopy)
                        tf.artworkFallbackReason = scoreReason.isNotEmpty()
                                                     ? scoreReason
                                                     : juce::String ("Spotify match rejected by local scoring");
                    else if (! artworkCachedCopy)
                        tf.artworkFallbackReason = "Spotify image cache failed";
                    break;
                }
                case PS::Discogs:
                {
                    const bool useArt = acceptedCopy && artworkCachedCopy;
                    tf.artworkSource      = useArt ? TrackFile::ArtworkSource::Discogs
                                                    : TrackFile::ArtworkSource::None;
                    tf.cachedArtworkPath  = useArt ? artFileCopy : juce::File();

                    tf.discogsStatus      = acceptedCopy
                                              ? TrackFile::DiscogsStatus::Found
                                              : TrackFile::DiscogsStatus::Uncertain;
                    tf.discogsArtist      = resultCopy.artist;
                    tf.discogsTitle       = resultCopy.title;
                    tf.discogsReleaseId   = resultCopy.discogsReleaseId;
                    tf.discogsCoverUrl    = resultCopy.imageUrl;
                    tf.discogsExternalUrl = resultCopy.discogsExternalUrl;
                    tf.discogsErrorReason.clear();

                    tf.spotifyStatus      = TrackFile::SpotifyStatus::NotSearched;
                    tf.spotifyErrorReason.clear();

                    if (! acceptedCopy)
                        tf.artworkFallbackReason = scoreReason.isNotEmpty()
                                                     ? scoreReason
                                                     : juce::String ("Discogs match rejected by local scoring");
                    else if (! artworkCachedCopy)
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
            outcome.rejectedByLocalScoring = ! scoreAccepted && proxyOk;

            if (accept && result.source == MetadataProxyClient::Source::Spotify && artworkCached)
                outcome.artworkSource = TrackFile::ArtworkSource::Spotify;
            else if (accept && result.source == MetadataProxyClient::Source::Discogs && artworkCached)
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
