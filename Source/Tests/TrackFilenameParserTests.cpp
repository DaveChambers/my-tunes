#include <juce_core/juce_core.h>

#include "../TrackFile.h"
#include "../TrackFilenameParser.h"

class TrackFilenameParserTests : public juce::UnitTest
{
public:
    TrackFilenameParserTests()
        : juce::UnitTest ("TrackFilenameParser", "my-tunes") {}

    void runTest() override
    {
        beginTest ("simple: 03A - 125 - Youandewan - Disarray");
        {
            TrackFile tf;
            TrackFilenameParser::parse (tf, juce::String ("03A - 125 - Youandewan - Disarray"));
            expect       (tf.filenameParseOk);
            expectEquals (tf.key,     juce::String ("03A"));
            expectEquals (tf.bpm,     125);
            expectEquals (tf.artist,  juce::String ("Youandewan"));
            expectEquals (tf.title,   juce::String ("Disarray"));
            expectEquals (tf.version, juce::String());
        }

        beginTest ("commas + internal hyphen in artist (Mo-T): not split");
        {
            TrackFile tf;
            TrackFilenameParser::parse (tf, juce::String (
                "04A - 125 - Thakzin, Mo-T, Mas Musiq - The Road is Long"));
            expect       (tf.filenameParseOk);
            expectEquals (tf.key,     juce::String ("04A"));
            expectEquals (tf.bpm,     125);
            expectEquals (tf.artist,  juce::String ("Thakzin, Mo-T, Mas Musiq"));
            expectEquals (tf.title,   juce::String ("The Road is Long"));
            expectEquals (tf.version, juce::String());
        }

        beginTest ("explicit version: Sticks & Stonez - You're My - Larse Remix");
        {
            TrackFile tf;
            TrackFilenameParser::parse (tf, juce::String (
                "04A - 125 - Sticks & Stonez - You're My - Larse Remix"));
            expect       (tf.filenameParseOk);
            expectEquals (tf.key,     juce::String ("04A"));
            expectEquals (tf.bpm,     125);
            expectEquals (tf.artist,  juce::String ("Sticks & Stonez"));
            expectEquals (tf.title,   juce::String ("You're My"));
            expectEquals (tf.version, juce::String ("Larse Remix"));
        }

        beginTest ("parenthesised remix info inside title: not promoted to version");
        {
            TrackFile tf;
            TrackFilenameParser::parse (tf, juce::String (
                "07A - 125 - Take One - Say My Name (I Don't Think So) (Morel And Bobby \"D\" Metro Mix)"));
            expect       (tf.filenameParseOk);
            expectEquals (tf.key,     juce::String ("07A"));
            expectEquals (tf.bpm,     125);
            expectEquals (tf.artist,  juce::String ("Take One"));
            expectEquals (tf.title,
                          juce::String ("Say My Name (I Don't Think So) (Morel And Bobby \"D\" Metro Mix)"));
            expectEquals (tf.version, juce::String());
        }

        beginTest ("more example filenames from Dave's folder");
        {
            struct Case
            {
                juce::String stem, key, artist, title, version;
                int bpm;
            };
            const Case cases[] = {
                { "08A - 125 - Weekend Players - Into The Sun",
                  "08A", "Weekend Players", "Into The Sun", "", 125 },
                { "05A - 125 - St Germain - Deep in It",
                  "05A", "St Germain", "Deep in It", "", 125 },
                { "06A - 125 - Twill Street - Skylight",
                  "06A", "Twill Street", "Skylight", "", 125 },
                { "12A - 125 - Tom Loves Rainbows - Mirage",
                  "12A", "Tom Loves Rainbows", "Mirage", "", 125 }
            };
            for (const auto& c : cases)
            {
                TrackFile tf;
                TrackFilenameParser::parse (tf, c.stem);
                expect       (tf.filenameParseOk, "parse should succeed for: " + c.stem);
                expectEquals (tf.key,     c.key);
                expectEquals (tf.bpm,     c.bpm);
                expectEquals (tf.artist,  c.artist);
                expectEquals (tf.title,   c.title);
                expectEquals (tf.version, c.version);
            }
        }

        beginTest ("rejects: too few segments");
        {
            TrackFile tf;
            TrackFilenameParser::parse (tf, juce::String ("just a filename"));
            expect (! tf.filenameParseOk);
        }

        beginTest ("rejects: non-numeric BPM");
        {
            TrackFile tf;
            TrackFilenameParser::parse (tf, juce::String ("04A - notabpm - Artist - Title"));
            expect (! tf.filenameParseOk);
        }

        beginTest ("supportedExtensions includes mp3 / wav / aiff / aif / flac / m4a");
        {
            const auto& exts = TrackFilenameParser::supportedExtensions();
            for (const auto* e : { "mp3", "wav", "aiff", "aif", "flac", "m4a" })
                expect (exts.contains (juce::String (e)),
                        juce::String ("expected supported extension: ") + e);
        }
    }
};

static TrackFilenameParserTests sTrackFilenameParserTests;
