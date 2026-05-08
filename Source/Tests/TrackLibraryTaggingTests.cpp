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
    }
};

static TrackLibraryTaggingTests sTrackLibraryTaggingTests;
