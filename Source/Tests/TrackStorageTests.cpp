#include <juce_core/juce_core.h>

#include "../TrackFile.h"
#include "../TrackLibrary.h"
#include "../TrackStorage.h"

namespace
{
    const juce::String kRave   = juce::String::fromUTF8 ("\xF0\x9F\x99\x82"); // 🙂
    const juce::String kSunset = juce::String::fromUTF8 ("\xF0\x9F\x8C\x85"); // 🌅
    const juce::String kMust   = juce::String::fromUTF8 ("\xE2\xAD\x90");     // ⭐

    TrackFile makeTrack (juce::String pathStr,
                         juce::String artist,
                         juce::String title)
    {
        TrackFile t;
        t.file     = juce::File (pathStr);
        t.filename = t.file.getFileName();
        t.artist   = std::move (artist);
        t.title    = std::move (title);
        t.filenameParseOk = true;
        return t;
    }
}

class TrackStorageTests : public juce::UnitTest
{
public:
    TrackStorageTests() : juce::UnitTest ("TrackStorage", "my-tunes") {}

    void runTest() override
    {
        beginTest ("save + load roundtrips emoji tags / priority / notes / Discogs");
        {
            const auto tempFile = juce::File::createTempFile (".json");

            // Build a library with state.
            TrackLibrary lib;
            lib.appendTrack (makeTrack ("/tunes/aaa.mp3", "Artist A", "Title A"));
            lib.appendTrack (makeTrack ("/tunes/bbb.mp3", "Artist B", "Title B"));
            lib.appendTrack (makeTrack ("/tunes/ccc.mp3", "Artist C", "Title C"));

            lib.toggleEmojiTag (0, kSunset);
            lib.toggleEmojiTag (0, kMust);
            lib.toggleEmojiTag (2, kRave);

            // Decorate track 1 with synthetic Discogs data + priority + notes.
            {
                auto& t = lib.getTrack (1);
                t.priority           = 4;
                t.notes              = "rolling vibe";
                t.discogsStatus      = TrackFile::DiscogsStatus::Found;
                t.discogsArtist      = "Discogs Artist";
                t.discogsTitle       = "Discogs Artist - Discogs Title";
                t.discogsReleaseId   = "98765";
                t.discogsCoverUrl    = "https://example.com/cover.jpg";
            }

            // Save to a custom location so we don't touch ~/Library/...
            {
                TrackStorage saver (tempFile);
                expect (saver.snapshotAndSave (lib));
                expect (tempFile.existsAsFile());
                expect (tempFile.getSize() > 0);
            }

            // Load into a fresh storage + library that has the same paths.
            TrackStorage loader (tempFile);
            expect (loader.load());

            TrackLibrary lib2;
            lib2.appendTrack (makeTrack ("/tunes/aaa.mp3", "Artist A", "Title A"));
            lib2.appendTrack (makeTrack ("/tunes/bbb.mp3", "Artist B", "Title B"));
            lib2.appendTrack (makeTrack ("/tunes/ccc.mp3", "Artist C", "Title C"));

            loader.applySavedFieldsToLibrary (lib2);

            const auto& a = lib2.getTrack (0);
            expect       (a.emojiTags.contains (kSunset));
            expect       (a.emojiTags.contains (kMust));
            expectEquals (a.emojiTags.size(), 2);

            const auto& b = lib2.getTrack (1);
            expectEquals (b.priority,            4);
            expectEquals (b.notes,               juce::String ("rolling vibe"));
            expect       (b.discogsStatus == TrackFile::DiscogsStatus::Found);
            expectEquals (b.discogsReleaseId,    juce::String ("98765"));
            expectEquals (b.discogsCoverUrl,
                          juce::String ("https://example.com/cover.jpg"));

            const auto& c = lib2.getTrack (2);
            expect       (c.emojiTags.contains (kRave));

            tempFile.deleteFile();
        }

        beginTest ("save + load roundtrips Spotify fields + artworkSource");
        {
            const auto tempFile = juce::File::createTempFile (".json");

            TrackLibrary lib;
            lib.appendTrack (makeTrack ("/tunes/spot.mp3", "S Artist", "S Title"));
            {
                auto& t = lib.getTrack (0);
                t.spotifyStatus      = TrackFile::SpotifyStatus::Found;
                t.spotifyTrackId     = "abc123";
                t.spotifyAlbumId     = "alb456";
                t.spotifyExternalUrl = "https://open.spotify.com/track/abc123";
                t.spotifyImageUrl    = "https://i.scdn.co/image/foo";
                t.spotifyArtist      = "Spotify Artist";
                t.spotifyTitle       = "Spotify Title";
                t.artworkSource      = TrackFile::ArtworkSource::Spotify;
                t.discogsStatus      = TrackFile::DiscogsStatus::NotSearched;
            }

            {
                TrackStorage saver (tempFile);
                expect (saver.snapshotAndSave (lib));
            }

            TrackLibrary lib2;
            lib2.appendTrack (makeTrack ("/tunes/spot.mp3", "S Artist", "S Title"));

            TrackStorage loader (tempFile);
            expect (loader.load());
            loader.applySavedFieldsToLibrary (lib2);

            const auto& t = lib2.getTrack (0);
            expect       (t.spotifyStatus == TrackFile::SpotifyStatus::Found);
            expectEquals (t.spotifyTrackId,     juce::String ("abc123"));
            expectEquals (t.spotifyAlbumId,     juce::String ("alb456"));
            expectEquals (t.spotifyExternalUrl,
                          juce::String ("https://open.spotify.com/track/abc123"));
            expectEquals (t.spotifyImageUrl,
                          juce::String ("https://i.scdn.co/image/foo"));
            expect       (t.artworkSource == TrackFile::ArtworkSource::Spotify);

            tempFile.deleteFile();
        }

        beginTest ("back-compat: old JSON with cached art + Discogs Found "
                   "infers artworkSource=Discogs on load");
        {
            const auto tempFile = juce::File::createTempFile (".json");

            // Hand-craft an "older" JSON file with no artworkSource key.
            const juce::String oldJson = R"({
                "version": 1,
                "folder":  "/tunes",
                "tracks":  [
                    { "path": "/tunes/old.mp3",
                      "filename": "old.mp3",
                      "tags": [],
                      "priority": 0,
                      "notes": "",
                      "discogsStatus":    "Found",
                      "discogsArtist":    "X",
                      "discogsTitle":     "Y",
                      "discogsReleaseId": "1",
                      "discogsCoverUrl":  "https://example.com/x.jpg",
                      "cachedArtworkPath": ")"
                + juce::File::createTempFile (".jpg").getFullPathName() +
                R"(" }
                ]
            })";

            // Make sure the cached art file we reference actually exists,
            // since applySavedFieldsToLibrary only restores cachedArtworkPath
            // if the file is present on disk.
            const auto json = juce::JSON::parse (oldJson);
            const auto cachedPath = json.getProperty ("tracks", juce::var())[0]
                                       .getProperty ("cachedArtworkPath", juce::var())
                                       .toString();
            juce::File (cachedPath).create();

            tempFile.replaceWithText (oldJson);

            TrackStorage loader (tempFile);
            expect (loader.load());

            TrackLibrary lib;
            lib.appendTrack (makeTrack ("/tunes/old.mp3", "X", "Y"));
            loader.applySavedFieldsToLibrary (lib);

            const auto& t = lib.getTrack (0);
            expect (t.artworkSource == TrackFile::ArtworkSource::Discogs);

            juce::File (cachedPath).deleteFile();
            tempFile.deleteFile();
        }

        beginTest ("load on a missing file is a no-op (returns false)");
        {
            const auto tempFile = juce::File::getCurrentWorkingDirectory()
                                    .getChildFile ("does-not-exist-mytunes-test.json");
            tempFile.deleteFile();

            TrackStorage loader (tempFile);
            expect (! loader.load());
        }

        beginTest ("apply only updates tracks whose path matches");
        {
            const auto tempFile = juce::File::createTempFile (".json");

            TrackLibrary lib;
            lib.appendTrack (makeTrack ("/tunes/old-path.mp3", "X", "Y"));
            lib.toggleEmojiTag (0, kRave);
            {
                TrackStorage saver (tempFile);
                expect (saver.snapshotAndSave (lib));
            }

            // Different path -> no carry-over.
            TrackLibrary lib2;
            lib2.appendTrack (makeTrack ("/tunes/new-path.mp3", "X", "Y"));

            TrackStorage loader (tempFile);
            expect (loader.load());
            loader.applySavedFieldsToLibrary (lib2);

            expect (lib2.getTrack (0).emojiTags.isEmpty());
            tempFile.deleteFile();
        }
    }
};

static TrackStorageTests sTrackStorageTests;
