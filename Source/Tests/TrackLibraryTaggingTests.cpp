#include <juce_core/juce_core.h>

#include "../TrackFile.h"
#include "../TrackLibrary.h"

namespace
{
    // The five emoji literals exposed by EmojiTagBoardComponent. Hard-coded
    // in tests to avoid pulling juce_gui_basics into the test target.
    const juce::String kRave    = juce::String::fromUTF8 ("\xF0\x9F\x99\x82"); // 🙂
    const juce::String kSunset  = juce::String::fromUTF8 ("\xF0\x9F\x8C\x85"); // 🌅
    const juce::String kPeak    = juce::String::fromUTF8 ("\xF0\x9F\x94\xA5"); // 🔥
    const juce::String kHouse   = juce::String::fromUTF8 ("\xF0\x9F\x95\xBA"); // 🕺
    const juce::String kMust    = juce::String::fromUTF8 ("\xE2\xAD\x90");     // ⭐

    TrackFile makeTrack (juce::String artist, juce::String title)
    {
        TrackFile t;
        t.artist = std::move (artist);
        t.title  = std::move (title);
        t.filenameParseOk = true;
        return t;
    }

    std::vector<int> tracksMatchingTag (const TrackLibrary& lib,
                                        const juce::String& emoji)
    {
        std::vector<int> out;
        for (int i = 0; i < lib.getNumTracks(); ++i)
            if (lib.getTrack (i).emojiTags.contains (emoji))
                out.push_back (i);
        return out;
    }
}

class TrackLibraryTaggingTests : public juce::UnitTest
{
public:
    TrackLibraryTaggingTests() : juce::UnitTest ("TrackLibrary tagging", "my-tunes") {}

    void runTest() override
    {
        beginTest ("toggleEmojiTag adds when absent, removes when present");
        {
            TrackLibrary lib;
            lib.appendTrack (makeTrack ("A", "First"));

            expect (! lib.getTrack (0).emojiTags.contains (kSunset));

            const bool nowHas = lib.toggleEmojiTag (0, kSunset);
            expect (nowHas);
            expect (lib.getTrack (0).emojiTags.contains (kSunset));

            const bool nowNot = lib.toggleEmojiTag (0, kSunset);
            expect (! nowNot);
            expect (! lib.getTrack (0).emojiTags.contains (kSunset));
        }

        beginTest ("toggleEmojiTag rejects out-of-range index without crash");
        {
            TrackLibrary lib;
            expect (! lib.toggleEmojiTag (-1, kRave));
            expect (! lib.toggleEmojiTag (0,  kRave));    // empty library
            expect (! lib.toggleEmojiTag (5,  kRave));
        }

        beginTest ("multiple tags coexist on a single track");
        {
            TrackLibrary lib;
            lib.appendTrack (makeTrack ("A", "First"));
            lib.toggleEmojiTag (0, kSunset);
            lib.toggleEmojiTag (0, kMust);

            const auto& tags = lib.getTrack (0).emojiTags;
            expect (tags.contains (kSunset));
            expect (tags.contains (kMust));
            expectEquals (tags.size(), 2);
        }

        beginTest ("countTracksWithTag matches the number of tagged tracks");
        {
            TrackLibrary lib;
            lib.appendTrack (makeTrack ("A", "1"));
            lib.appendTrack (makeTrack ("B", "2"));
            lib.appendTrack (makeTrack ("C", "3"));

            lib.toggleEmojiTag (0, kSunset);
            lib.toggleEmojiTag (2, kSunset);

            expectEquals (lib.countTracksWithTag (kSunset), 2);
            expectEquals (lib.countTracksWithTag (kRave),   0);
        }

        beginTest ("filter-by-tag picks out matching tracks in original order");
        {
            TrackLibrary lib;
            lib.appendTrack (makeTrack ("A", "1"));   // index 0
            lib.appendTrack (makeTrack ("B", "2"));   // index 1
            lib.appendTrack (makeTrack ("C", "3"));   // index 2
            lib.appendTrack (makeTrack ("D", "4"));   // index 3

            lib.toggleEmojiTag (0, kPeak);
            lib.toggleEmojiTag (2, kPeak);
            lib.toggleEmojiTag (3, kPeak);

            const auto matched = tracksMatchingTag (lib, kPeak);
            expectEquals ((int) matched.size(), 3);
            if (matched.size() == 3)
            {
                expectEquals (matched[0], 0);
                expectEquals (matched[1], 2);
                expectEquals (matched[2], 3);
            }

            const auto noMatch = tracksMatchingTag (lib, kHouse);
            expect (noMatch.empty());
        }

        // (Intentionally not unit-testing the async ChangeBroadcaster
        //  delivery here — driving JUCE's message queue from a
        //  console-mode test is fiddly. The end-to-end wiring is
        //  exercised when the app runs: a tag toggle triggers a
        //  changeListenerCallback on each view + on the MainComponent,
        //  which Dave will see immediately on right-click.)

        beginTest ("bulk-load API: clear + appendBatchSilently scales to many tracks");
        {
            TrackLibrary lib;

            // Start with one track + folder set, simulating a previous scan.
            lib.appendTrack (makeTrack ("Old", "Track"));
            lib.toggleEmojiTag (0, kRave);
            expect (lib.getNumTracks() == 1);

            // clear() resets the library + folder, and broadcasts once.
            const juce::File newFolder ("/tunes/new");
            lib.clear (newFolder);
            expect (lib.getNumTracks() == 0);
            expect (lib.getFolder() == newFolder);

            // appendBatchSilently moves a batch in without broadcasting
            // per-call. Two batches of 2,500 each = 5,000 tracks.
            for (int batchIdx = 0; batchIdx < 2; ++batchIdx)
            {
                std::vector<TrackFile> batch;
                batch.reserve (2500);
                for (int i = 0; i < 2500; ++i)
                    batch.push_back (makeTrack (
                        "Artist " + juce::String (batchIdx * 2500 + i),
                        "Title "  + juce::String (batchIdx * 2500 + i)));
                lib.appendBatchSilently (std::move (batch));
            }
            expect (lib.getNumTracks() == 5000);

            // Indices are stable and contiguous.
            expectEquals (lib.getTrack (0).artist,    juce::String ("Artist 0"));
            expectEquals (lib.getTrack (2499).artist, juce::String ("Artist 2499"));
            expectEquals (lib.getTrack (2500).artist, juce::String ("Artist 2500"));
            expectEquals (lib.getTrack (4999).artist, juce::String ("Artist 4999"));
        }
    }
};

static TrackLibraryTaggingTests sTrackLibraryTaggingTests;
