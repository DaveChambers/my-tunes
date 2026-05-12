#include "TrackFilenameParser.h"

const juce::StringArray& TrackFilenameParser::supportedExtensions()
{
    static const juce::StringArray exts {
        "mp3", "wav", "aiff", "aif", "flac", "m4a"
    };
    return exts;
}

namespace
{
    // Split `s` on every occurrence of the literal three-character token
    // " - ". juce::StringArray::addTokens treats its delimiter as a *set
    // of characters*, which is not what we want.
    juce::StringArray splitOnDashSeparator (const juce::String& s)
    {
        juce::StringArray parts;
        const juce::String sep (" - ");
        const int sepLen = sep.length();

        int searchFrom = 0;
        while (searchFrom <= s.length())
        {
            const int next = s.indexOf (searchFrom, sep);
            if (next < 0)
            {
                parts.add (s.substring (searchFrom));
                break;
            }
            parts.add (s.substring (searchFrom, next));
            searchFrom = next + sepLen;
        }
        for (auto& p : parts) p = p.trim();
        return parts;
    }
}

void TrackFilenameParser::parse (TrackFile& tf,
                                  const juce::String& filenameWithoutExtension)
{
    tf.filenameParseOk = false;
    tf.key.clear();
    tf.bpm = 0;
    tf.artist.clear();
    tf.title.clear();
    tf.version.clear();

    auto parts = splitOnDashSeparator (filenameWithoutExtension);
    if (parts.size() < 4)
        return;

    const auto bpmValue = parts[1].getIntValue();
    if (bpmValue <= 0)
        return;

    tf.key    = parts[0];
    tf.bpm    = bpmValue;
    tf.artist = parts[2];

    if (parts.size() == 4)
    {
        tf.title   = parts[3];
        tf.version = juce::String();
    }
    else
    {
        // 5+ parts: treat the last segment as the version, the segments
        // between artist and version as the title joined back with " - ".
        juce::StringArray titleParts;
        for (int i = 3; i < parts.size() - 1; ++i)
            titleParts.add (parts[i]);
        tf.title   = titleParts.joinIntoString (" - ");
        tf.version = parts[parts.size() - 1];
    }

    tf.filenameParseOk = ! tf.title.isEmpty() && ! tf.artist.isEmpty();
}

void TrackFilenameParser::parse (TrackFile& tf, const juce::File& file)
{
    tf.file      = file;
    tf.filename  = file.getFileName();
    tf.extension = file.getFileExtension().substring (1).toLowerCase();
    parse (tf, file.getFileNameWithoutExtension());
    tf.rebuildSearchKey();
}
