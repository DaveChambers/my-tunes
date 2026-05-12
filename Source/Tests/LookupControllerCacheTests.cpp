#include <juce_core/juce_core.h>

#include "../LookupController.h"
#include "../TrackFile.h"

// Tests for LookupController's cache-skip predicate. The predicate is
// the load-bearing piece of "don't hammer the metadata service for
// tracks we've already resolved" — get this wrong and an 8 000-track
// library re-queries the Cloud Run proxy on every launch.
class LookupControllerCacheTests : public juce::UnitTest
{
public:
    LookupControllerCacheTests() : juce::UnitTest ("LookupController.cache", "my-tunes") {}

    void runTest() override
    {
        // Create a real, openable file so cachedArtworkPath.existsAsFile()
        // returns the genuine answer.
        const auto realFile = juce::File::createTempFile (".jpg");
        realFile.create();
        const juce::File missingFile ("/this/path/definitely/does/not/exist.jpg");

        auto trackWithArt = [&] (TrackFile::ArtworkSource src, juce::File cache)
        {
            TrackFile t;
            t.artist = "X"; t.title = "Y";
            t.artworkSource     = src;
            t.cachedArtworkPath = std::move (cache);
            return t;
        };

        beginTest ("never-resolved track must be enqueued for lookup");
        {
            const auto t = trackWithArt (TrackFile::ArtworkSource::None, juce::File());
            expect (  LookupController::shouldEnqueueForLookup (t, false));
            expect (  LookupController::shouldEnqueueForLookup (t, true));
        }

        beginTest ("track with cached Spotify artwork is SKIPPED on normal lookup");
        {
            const auto t = trackWithArt (TrackFile::ArtworkSource::Spotify, realFile);
            expect (! LookupController::shouldEnqueueForLookup (t, false));
        }

        beginTest ("track with cached Discogs artwork is SKIPPED on normal lookup");
        {
            const auto t = trackWithArt (TrackFile::ArtworkSource::Discogs, realFile);
            expect (! LookupController::shouldEnqueueForLookup (t, false));
        }

        beginTest ("track whose cached artwork file has been DELETED is enqueued");
        {
            // The previous session resolved artwork, but someone wiped
            // ~/Library/My Tunes/ArtworkCache/<hash>.jpg between
            // sessions. We should re-fetch.
            const auto t = trackWithArt (TrackFile::ArtworkSource::Spotify, missingFile);
            expect (LookupController::shouldEnqueueForLookup (t, false));
        }

        beginTest ("track marked artworkSource=Spotify with EMPTY cached path is enqueued");
        {
            const auto t = trackWithArt (TrackFile::ArtworkSource::Spotify, juce::File());
            expect (LookupController::shouldEnqueueForLookup (t, false));
        }

        beginTest ("Re-lookup (forceRelookup=true) ALWAYS enqueues, even cached tracks");
        {
            const auto cached  = trackWithArt (TrackFile::ArtworkSource::Spotify, realFile);
            const auto missing = trackWithArt (TrackFile::ArtworkSource::None,    juce::File());
            expect (LookupController::shouldEnqueueForLookup (cached,  true));
            expect (LookupController::shouldEnqueueForLookup (missing, true));
        }

        realFile.deleteFile();
    }
};

static LookupControllerCacheTests sLookupControllerCacheTests;
