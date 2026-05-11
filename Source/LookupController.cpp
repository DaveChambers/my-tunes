#include "LookupController.h"

#include "TrackLibrary.h"

LookupController::LookupController (TrackLibrary& l,
                                     SpotifyClient& s,
                                     DiscogsClient& d,
                                     ArtworkCache&  a)
    : juce::Thread ("TuneList-Lookup"),
      library (l), spotify (s), discogs (d), artwork (a)
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

        // Cache: skip tracks that already have artwork from any source
        // with a real cached file on disk. Re-lookup bypasses this.
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
        juce::String ("Lookup: starting batch of ") + juce::String (total)
        + " track(s); Spotify creds="
        + (spotify.hasCredentials() ? "yes" : "no")
        + ", Discogs token="
        + (discogs.hasToken() ? "yes" : "no"));

    for (auto& task : tasks)
    {
        if (threadShouldExit())
            break;

        const int idx = task.index;
        juce::Logger::writeToLog (
            "Lookup[" + juce::String (idx) + "]: artist=\"" + task.artist
            + "\" title=\"" + task.title + "\"");

        // 1) Mark both statuses as Searching on the message thread so
        //    the UI can show "(searching...)" placeholders. We don't
        //    yet know which source will win.
        juce::MessageManager::callAsync ([&libraryRef, idx]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);
            tf.spotifyStatus = TrackFile::SpotifyStatus::Searching;
            tf.discogsStatus = TrackFile::DiscogsStatus::Searching;
            libraryRef.sendChangeMessage();
        });

        // 2) Spotify first (if credentials are available).
        SpotifyClient::Result spotifyResult;
        juce::File            spotifyArtFile;
        bool                  spotifyWins = false;
        juce::String          fallbackReason;

        if (spotify.hasCredentials())
        {
            spotifyResult = spotify.lookup (task.artist, task.title);

            // A Spotify match only "wins" if we got an image URL AND
            // managed to cache its bytes locally. A Found-without-image
            // result is treated as a miss so Discogs gets to try.
            const bool spotifyHadImageMatch =
                (spotifyResult.status == TrackFile::SpotifyStatus::Found
              || spotifyResult.status == TrackFile::SpotifyStatus::Uncertain)
                && spotifyResult.imageUrl.isNotEmpty();

            if (spotifyHadImageMatch)
            {
                spotifyArtFile = artwork.ensureCachedFile (spotifyResult.imageUrl);
                spotifyWins = spotifyArtFile.existsAsFile()
                              && spotifyArtFile.getSize() > 0;
                if (! spotifyWins)
                    fallbackReason = "Spotify image cache failed";
                juce::Logger::writeToLog (
                    spotifyWins
                      ? juce::String ("Lookup[" + juce::String (idx) + "]: Spotify image cached OK")
                      : juce::String ("Lookup[" + juce::String (idx) + "]: Spotify image cache FAILED -> Discogs fallback"));
            }
            else
            {
                fallbackReason = "Spotify " + TrackFile::spotifyStatusText (spotifyResult.status);
                if (spotifyResult.status == TrackFile::SpotifyStatus::Error
                    && spotifyResult.errorReason.isNotEmpty())
                    fallbackReason += " (" + spotifyResult.errorReason + ")";
                else if ((spotifyResult.status == TrackFile::SpotifyStatus::Found
                       || spotifyResult.status == TrackFile::SpotifyStatus::Uncertain)
                       && spotifyResult.imageUrl.isEmpty())
                    fallbackReason = "Spotify match had no album image";

                juce::Logger::writeToLog (
                    "Lookup[" + juce::String (idx) + "]: Spotify "
                    + TrackFile::spotifyStatusText (spotifyResult.status)
                    + " -> Discogs fallback");
            }
        }
        else
        {
            spotifyResult.status      = TrackFile::SpotifyStatus::CredentialsMissing;
            spotifyResult.errorReason = "Spotify credentials not configured";
            fallbackReason = "Spotify credentials missing";
            juce::Logger::writeToLog (
                "Lookup[" + juce::String (idx)
                + "]: Spotify creds missing -> Discogs fallback");
        }

        // 3) Discogs fallback if Spotify didn't win.
        DiscogsClient::Result discogsResult;
        juce::File            discogsArtFile;
        bool                  discogsRan = false;
        bool                  discogsWins = false;

        if (! spotifyWins)
        {
            discogsResult = discogs.lookup (task.artist, task.title);
            discogsRan    = true;
            juce::Logger::writeToLog (
                "Lookup[" + juce::String (idx) + "]: Discogs "
                + TrackFile::discogsStatusText (discogsResult.status)
                + (discogsResult.errorReason.isNotEmpty()
                    ? juce::String (" (") + discogsResult.errorReason + ")"
                    : juce::String()));

            const auto imgUrl = discogsResult.thumbUrl.isNotEmpty()
                                  ? discogsResult.thumbUrl
                                  : discogsResult.coverUrl;
            const bool discogsHadImageMatch =
                (discogsResult.status == TrackFile::DiscogsStatus::Found
              || discogsResult.status == TrackFile::DiscogsStatus::Uncertain)
                && imgUrl.isNotEmpty();

            if (discogsHadImageMatch)
            {
                discogsArtFile = artwork.ensureCachedFile (imgUrl);
                discogsWins    = discogsArtFile.existsAsFile()
                                  && discogsArtFile.getSize() > 0;
            }
        }
        else
        {
            // Spotify won — keep whatever previous Discogs state was on
            // the track. We'll restore it on the message thread.
            discogsResult.status = TrackFile::DiscogsStatus::NotSearched;
        }

        // 4) Apply on the message thread.
        const auto spotifyApplied = spotifyResult;
        const auto discogsApplied = discogsResult;
        const auto spotifyPath    = spotifyArtFile;
        const auto discogsPath    = discogsArtFile;
        const bool spotifyWinsCopy = spotifyWins;
        const bool discogsWinsCopy = discogsWins;
        const bool discogsRanCopy  = discogsRan;
        const auto fallbackReasonCopy = fallbackReason;

        juce::MessageManager::callAsync ([&libraryRef, idx, spotifyApplied,
                                          discogsApplied, spotifyPath, discogsPath,
                                          spotifyWinsCopy, discogsWinsCopy,
                                          discogsRanCopy, fallbackReasonCopy]
        {
            if (idx < 0 || idx >= libraryRef.getNumTracks()) return;
            auto& tf = libraryRef.getTrack (idx);

            // Spotify side
            tf.spotifyStatus      = spotifyApplied.status;
            tf.spotifyTrackId     = spotifyApplied.trackId;
            tf.spotifyAlbumId     = spotifyApplied.albumId;
            tf.spotifyExternalUrl = spotifyApplied.externalUrl;
            tf.spotifyImageUrl    = spotifyApplied.imageUrl;
            tf.spotifyArtist      = spotifyApplied.matchedArtist;
            tf.spotifyTitle       = spotifyApplied.matchedTitle;
            tf.spotifyAlbum       = spotifyApplied.matchedAlbum;
            tf.spotifyQuery       = spotifyApplied.queryUsed;
            tf.spotifyErrorReason = spotifyApplied.errorReason;
            tf.artworkFallbackReason = spotifyWinsCopy ? juce::String()
                                                        : fallbackReasonCopy;

            // Discogs side — only overwrite if we actually queried it
            // (i.e. Spotify didn't win). Otherwise preserve previous
            // state so an old "Found" doesn't get clobbered to
            // NotSearched just because we skipped Discogs.
            if (discogsRanCopy)
            {
                tf.discogsStatus      = discogsApplied.status;
                tf.discogsArtist      = discogsApplied.matchedArtist;
                tf.discogsTitle       = discogsApplied.matchedTitle;
                tf.discogsReleaseId   = discogsApplied.releaseId;
                tf.discogsCoverUrl    = discogsApplied.coverUrl;
                tf.discogsErrorReason = discogsApplied.errorReason;
            }
            else
            {
                tf.discogsStatus = TrackFile::DiscogsStatus::NotSearched;
                tf.discogsErrorReason.clear();
            }

            // Pick the winning artwork path + source.
            if (spotifyWinsCopy)
            {
                tf.artworkSource     = TrackFile::ArtworkSource::Spotify;
                tf.cachedArtworkPath = spotifyPath;
            }
            else if (discogsWinsCopy)
            {
                tf.artworkSource     = TrackFile::ArtworkSource::Discogs;
                tf.cachedArtworkPath = discogsPath;
            }
            else
            {
                tf.artworkSource     = TrackFile::ArtworkSource::None;
                tf.cachedArtworkPath = juce::File();
            }

            libraryRef.sendChangeMessage();
        });

        // 4b) Per-track callback — fold the dual-source result into a
        //     single outcome struct so MainComponent can keep a clean
        //     accumulator.
        if (onTrackProcessed)
        {
            TrackOutcome outcome;
            outcome.spotifyStatus = spotifyApplied.status;
            outcome.discogsStatus = discogsRanCopy
                                      ? discogsApplied.status
                                      : TrackFile::DiscogsStatus::NotSearched;
            outcome.spotifyError  = spotifyApplied.errorReason;
            outcome.discogsError  = discogsRanCopy ? discogsApplied.errorReason
                                                    : juce::String();
            outcome.artworkSource = spotifyWinsCopy
                                      ? TrackFile::ArtworkSource::Spotify
                                      : (discogsWinsCopy
                                           ? TrackFile::ArtworkSource::Discogs
                                           : TrackFile::ArtworkSource::None);

            const auto cb        = onTrackProcessed;
            const int  trackIdx  = idx;
            juce::MessageManager::callAsync ([cb, trackIdx, outcome]
            {
                if (cb) cb (trackIdx, outcome);
            });
        }

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

        juce::Logger::writeToLog (
            "Lookup[" + juce::String (idx) + "]: final artworkSource="
            + (spotifyWinsCopy ? juce::String ("Spotify")
               : (discogsWinsCopy ? juce::String ("Discogs")
                                  : juce::String ("None"))));

        // 6) Be nice to both APIs.
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
