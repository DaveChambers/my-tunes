#include "LookupMatching.h"

namespace LookupMatching
{

namespace
{
    const juce::StringArray& extensions()
    {
        static const juce::StringArray exts {
            "mp3","wav","aif","aiff","flac","m4a","aac","ogg",
            "opus","alac","caf","wma"
        };
        return exts;
    }

    const juce::StringArray& stopwords()
    {
        // These get stripped during tokenisation. Crucially this
        // includes "and" (so "Phonat And Super Mal" doesn't accidentally
        // overlap with a returned artist that contains "and") and the
        // common version-tag words (so "(Original Mix)" doesn't bias
        // the match by itself).
        static const juce::StringArray words {
            "the","and","feat","feat.","ft","ft.","vs","vs.","with",
            "remix","mix","original","edit","version","extended","radio"
        };
        return words;
    }

    // Strip a trailing audio-file extension, tolerating junk
    // characters tacked on after the dot. "You Girls.wav~~" -> "You Girls".
    juce::String stripFileExtensionLeniently (const juce::String& s)
    {
        const int dot = s.lastIndexOfChar ('.');
        if (dot < 0) return s;

        auto tail = s.substring (dot + 1);
        // Drop trailing non-alphanumeric junk so ".wav~~" reads as "wav".
        while (tail.isNotEmpty()
               && ! juce::CharacterFunctions::isLetterOrDigit (tail.getLastCharacter()))
            tail = tail.dropLastCharacters (1);

        if (tail.isEmpty() || tail.length() > 6)
            return s;

        if (extensions().contains (tail.toLowerCase()))
            return s.substring (0, dot);

        return s;
    }

    juce::String collapseWhitespace (const juce::String& s)
    {
        // Normalise tabs / newlines / vertical-tabs to plain spaces
        // first, then collapse runs of spaces. (juce::String::trim
        // already handles all whitespace on the ends.)
        auto out = s.replaceCharacters ("\t\n\r\v\f", "     ");
        while (out.contains ("  "))
            out = out.replace ("  ", " ");
        return out.trim();
    }
}

juce::String cleanForLookup (const juce::String& raw)
{
    auto s = stripFileExtensionLeniently (raw);

    // Strip any trailing non-alphanumeric junk (~, @, %, _, -, space,
    // etc). Closing brackets / parens are preserved so legitimate
    // titles like "Pixelated (Original Mix)" keep their bracket.
    while (s.isNotEmpty())
    {
        const auto c = s.getLastCharacter();
        if (juce::CharacterFunctions::isLetterOrDigit (c)
            || c == ')' || c == ']' || c == '}')
            break;
        s = s.dropLastCharacters (1);
    }

    return collapseWhitespace (s);
}

juce::StringArray tokenize (const juce::String& s)
{
    // Replace every non-alphanumeric character with a space, then
    // tokenise by whitespace. juce::String iteration is UTF-8 safe.
    juce::String flat;
    flat.preallocateBytes ((std::size_t) s.getNumBytesAsUTF8());
    for (auto it = s.begin(); it != s.end(); ++it)
    {
        const auto c = *it;
        if (juce::CharacterFunctions::isLetterOrDigit (c))
            flat += juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));
        else
            flat += ' ';
    }

    juce::StringArray tokens;
    tokens.addTokens (flat, " ", "");
    tokens.removeEmptyStrings();

    for (int i = tokens.size() - 1; i >= 0; --i)
    {
        const auto& t = tokens[i];
        if (t.length() < 2 || stopwords().contains (t))
            tokens.remove (i);
    }

    return tokens;
}

const char* toString (Confidence c) noexcept
{
    switch (c)
    {
        case Confidence::Rejected: return "Rejected";
        case Confidence::Medium:   return "Medium";
        case Confidence::High:     return "High";
    }
    return "Rejected";
}

namespace
{
    int overlapCount (const juce::StringArray& a, const juce::StringArray& b)
    {
        int n = 0;
        for (const auto& t : a)
            if (b.contains (t))
                ++n;
        return n;
    }
}

Score scoreMatch (const juce::String& inputArtist,
                  const juce::String& inputTitle,
                  const juce::String& returnedArtist,
                  const juce::String& returnedTitle)
{
    Score s;

    const auto inArtist  = tokenize (inputArtist);
    const auto inTitle   = tokenize (inputTitle);
    const auto retArtist = tokenize (returnedArtist);
    const auto retTitle  = tokenize (returnedTitle);

    const int artistOverlap = overlapCount (inArtist, retArtist);
    const int titleOverlap  = overlapCount (inTitle,  retTitle);

    if (inArtist.size() > 0 && artistOverlap == 0)
    {
        s.confidence = Confidence::Rejected;
        s.reason = "artist mismatch (\"" + inputArtist
                   + "\" vs \"" + returnedArtist + "\")";
        return s;
    }
    if (inTitle.size() > 0 && titleOverlap == 0)
    {
        s.confidence = Confidence::Rejected;
        s.reason = "title mismatch (\"" + inputTitle
                   + "\" vs \"" + returnedTitle + "\")";
        return s;
    }

    // If the input had no usable tokens on a side, we couldn't judge
    // — fall to Medium rather than Reject so we don't punish badly
    // parsed filenames more than necessary.
    const bool artistAllMatched = (inArtist.size() > 0
                                    && artistOverlap == inArtist.size());
    const bool titleAllMatched  = (inTitle.size() > 0
                                    && titleOverlap == inTitle.size());

    if (artistAllMatched && titleAllMatched)
        s.confidence = Confidence::High;
    else
        s.confidence = Confidence::Medium;

    return s;
}

} // namespace LookupMatching
