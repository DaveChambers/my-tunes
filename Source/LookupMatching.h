#pragma once

#include <juce_core/juce_core.h>

// Helpers for cleaning artist/title strings before sending them to the
// metadata service and for deciding whether a returned result is good
// enough to use as the track's artwork.
//
// Why both halves are needed:
//   - The cleaning half stops obvious garbage (file extensions, trailing
//     `~~` / `@@`, double spaces) from poisoning the query string.
//   - The scoring half guards against a much bigger class of misses:
//     the Spotify/Discogs proxy always returns *something*, even when
//     nothing close exists. Without local scoring the app happily
//     paints unrelated artwork onto Dave's tracks.
//
// Both functions are deterministic, side-effect-free, and unit-tested.
namespace LookupMatching
{
    // Returns `raw` with:
    //   - trailing audio-file extension (.mp3 / .wav / .aif / .aiff /
    //     .flac / .m4a / .aac / .ogg / .opus / .alac / .caf / .wma)
    //     stripped, tolerant of trailing junk runs after the dot
    //     ("You Girls.wav~~" -> "You Girls"),
    //   - any trailing non-alphanumeric junk characters trimmed
    //     ("Pixelated (Original Mix)@@" -> "Pixelated (Original Mix)"),
    //   - repeated whitespace collapsed to single spaces,
    //   - leading + trailing whitespace removed.
    juce::String cleanForLookup (const juce::String& raw);

    // Confidence label for the match between the user's input (the
    // parsed artist/title from the filename) and what the proxy
    // returned. Anything other than High/Medium means the desktop will
    // refuse to show the artwork.
    enum class Confidence { Rejected, Medium, High };

    const char* toString (Confidence) noexcept;

    struct Score
    {
        Confidence   confidence = Confidence::Rejected;
        // Human-readable diagnostic explaining a Rejected / Medium
        // outcome. Empty for High. Surfaced to Console.app on the
        // lookup log line and (briefly) to the detail panel.
        juce::String reason;
    };

    // Compare the cleaned input artist/title against the artist/title
    // the proxy returned. Tokenisation:
    //   - lowercase,
    //   - non-alphanumeric -> spaces,
    //   - drop tokens of length < 2,
    //   - drop common stopwords (the, and, feat, ft, vs, with, x,
    //     remix, mix, original, edit, version, extended, radio).
    //
    // Rules:
    //   - empty input token set on either side: cannot judge -> Medium
    //     (so a track filename that gave us no artist still gets a
    //     fair chance based on title alone, and vice versa);
    //   - non-empty input + zero overlap on artist OR on title:
    //     Rejected with a reason naming which side failed;
    //   - all input tokens present in the returned tokens for BOTH
    //     sides: High;
    //   - some overlap on both sides but not all input tokens
    //     matched: Medium.
    Score scoreMatch (const juce::String& inputArtist,
                      const juce::String& inputTitle,
                      const juce::String& returnedArtist,
                      const juce::String& returnedTitle);

    // Tokenise visible for tests; not part of the public matching
    // contract — kept in the header so the cleaning + tokenising can
    // be exercised directly.
    juce::StringArray tokenize (const juce::String& s);
}
