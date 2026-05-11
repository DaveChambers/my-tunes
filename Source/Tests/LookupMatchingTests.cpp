#include <juce_core/juce_core.h>

#include "../LookupMatching.h"

// Tests for LookupMatching::cleanForLookup and ::scoreMatch.
// These cover the exact bad-match cases Dave reported (Nush / "You
// Girls.wav~~" -> Shatta Wale "Gringo"; Phonat And Super Mal /
// "Pixelated (Original Mix)@@" -> "Super Jon-Z"), plus a handful of
// positive cases so the threshold doesn't reject obviously-correct
// matches like Phil Kieran / Saturdays.
class LookupMatchingTests : public juce::UnitTest
{
public:
    LookupMatchingTests() : juce::UnitTest ("LookupMatching", "my-tunes") {}

    void runTest() override
    {
        using namespace LookupMatching;

        beginTest ("cleanForLookup strips file extensions");
        {
            expectEquals (cleanForLookup ("You Girls.wav"),  juce::String ("You Girls"));
            expectEquals (cleanForLookup ("Track.mp3"),       juce::String ("Track"));
            expectEquals (cleanForLookup ("song.flac"),       juce::String ("song"));
        }

        beginTest ("cleanForLookup strips trailing junk after extension");
        {
            expectEquals (cleanForLookup ("You Girls.wav~~"),    juce::String ("You Girls"));
            expectEquals (cleanForLookup ("Pixelated (Original Mix)@@"),
                          juce::String ("Pixelated (Original Mix)"));
            expectEquals (cleanForLookup ("Track.mp3@@~"),       juce::String ("Track"));
        }

        beginTest ("cleanForLookup collapses repeated whitespace");
        {
            expectEquals (cleanForLookup ("  Phil   Kieran  "), juce::String ("Phil Kieran"));
            expectEquals (cleanForLookup ("A\tB\tC"),            juce::String ("A B C"));
        }

        beginTest ("cleanForLookup preserves legitimate closing brackets");
        {
            expectEquals (cleanForLookup ("Pixelated (Original Mix)"),
                          juce::String ("Pixelated (Original Mix)"));
            expectEquals (cleanForLookup ("Title [Remix]"),
                          juce::String ("Title [Remix]"));
        }

        beginTest ("tokenize drops stopwords and short tokens");
        {
            const auto t = tokenize ("Phonat And Super Mal");
            expect (! t.contains ("and"));
            expect (  t.contains ("phonat"));
            expect (  t.contains ("super"));
            expect (  t.contains ("mal"));

            const auto t2 = tokenize ("Pixelated (Original Mix)");
            expect (  t2.contains ("pixelated"));
            expect (! t2.contains ("original"));
            expect (! t2.contains ("mix"));
        }

        beginTest ("scoreMatch: identical strings score High");
        {
            const auto s = scoreMatch ("Phil Kieran", "Saturdays",
                                       "Phil Kieran", "Saturdays");
            expect (s.confidence == Confidence::High);
        }

        beginTest ("scoreMatch: partial artist overlap scores Medium");
        {
            const auto s = scoreMatch ("Phonat And Super Mal", "Pixelated",
                                       "Phonat",               "Pixelated");
            expect (s.confidence == Confidence::Medium);
        }

        beginTest ("scoreMatch: title remix annotation in input still accepts");
        {
            // Input has "(Original Mix)" tags that get stopworded away;
            // returned title is bare "Pixelated". Should still accept.
            const auto s = scoreMatch ("Phonat", "Pixelated (Original Mix)",
                                       "Phonat", "Pixelated");
            expect (s.confidence != Confidence::Rejected);
        }

        beginTest ("scoreMatch: Nush / You Girls -> Shatta Wale / Gringo is Rejected");
        {
            const auto s = scoreMatch ("Nush",        "You Girls",
                                       "Shatta Wale", "Gringo");
            expect       (s.confidence == Confidence::Rejected);
            expect       (s.reason.containsIgnoreCase ("artist"));
        }

        beginTest ("scoreMatch: Nush / You Girls -> Maroon 5 / Girls Like You is Rejected");
        {
            // Even though "girls" overlaps in titles, artist is wildly
            // different, so the result must still be rejected.
            const auto s = scoreMatch ("Nush",     "You Girls",
                                       "Maroon 5", "Girls Like You");
            expect (s.confidence == Confidence::Rejected);
            expect (s.reason.containsIgnoreCase ("artist"));
        }

        beginTest ("scoreMatch: Phonat And Super Mal / Pixelated -> Jon Z / "
                   "Super Jon-Z is Rejected");
        {
            const auto s = scoreMatch ("Phonat And Super Mal", "Pixelated",
                                       "Jon Z",                "Super Jon-Z (Residente Challenge)");
            // Artist "Phonat / Super / Mal" vs "Jon" -> no artist token
            // overlap -> Rejected.
            expect (s.confidence == Confidence::Rejected);
            expect (s.reason.containsIgnoreCase ("artist"));
        }

        beginTest ("scoreMatch: Phonat And Super Mal / Pixelated -> Eeveelution "
                   "ensemble is Rejected on title");
        {
            // Artist "Max Austin, Jixplosion, ..." doesn't contain any
            // of the input tokens either, but in any case the title
            // "Eeveelution" has zero overlap with "Pixelated".
            const auto s = scoreMatch ("Phonat And Super Mal", "Pixelated",
                                       "Max Austin, Jixplosion, TrayeFreezy",
                                       "Eeveelution");
            expect (s.confidence == Confidence::Rejected);
        }

        beginTest ("scoreMatch: Mo-T short-token artist still matches itself");
        {
            // "Mo-T" tokenises to ["mo"] after the length>=2 filter.
            // Same on the returned side. Should be Medium (title side
            // is not perfect substring) but NOT Rejected.
            const auto s = scoreMatch ("Mo-T", "Don't U Want Me",
                                       "Mo-T", "Don't U Want Me (Original Mix)");
            expect (s.confidence != Confidence::Rejected);
        }
    }
};

static LookupMatchingTests sLookupMatchingTests;
