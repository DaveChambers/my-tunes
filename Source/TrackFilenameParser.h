#pragma once

#include <juce_core/juce_core.h>

#include "TrackFile.h"

// Tolerant parser for filenames in Dave's DJ-tune naming convention:
//
//     KEY - BPM - Artist - Title.ext
//     KEY - BPM - Artist - Title - Optional Version.ext
//
// Splits on the literal token " - " (space-dash-space). Normal hyphens
// inside an artist or title (e.g. "Mo-T") have no surrounding spaces and
// therefore aren't treated as separators.
class TrackFilenameParser
{
public:
    // Parse from the filename's stem (no extension) and fill the parsed
    // fields on `tf`. Sets `tf.filenameParseOk` true if parsing produced a
    // non-empty artist/title and a positive BPM. Does NOT touch
    // tf.file / tf.filename / tf.extension.
    static void parse (TrackFile& tf, const juce::String& filenameWithoutExtension);

    // Convenience: also fills tf.file / tf.filename / tf.extension from the
    // juce::File, then runs parse() against the file's stem.
    static void parse (TrackFile& tf, const juce::File& file);

    // Lower-case audio extensions (no leading dot) we recognise as tracks.
    static const juce::StringArray& supportedExtensions();
};
