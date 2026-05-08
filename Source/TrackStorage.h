#pragma once

#include <juce_core/juce_core.h>

#include "TrackFile.h"

class TrackLibrary;

// Persistence to a single JSON file under
//   ~/Library/Application Support/My Tunes/my-tunes-library.json
// Stored: folder path, parsed metadata, emoji tags, priority, notes,
// Discogs metadata, local cached artwork path.
//
// Stub for the skeleton commit; real load/save lives in a later commit.
class TrackStorage
{
public:
    TrackStorage();
    ~TrackStorage();

    // Path the library JSON would land at — exposed so the UI / About
    // dialog can show it. Created lazily on first save.
    static juce::File getLibraryFile();

    // Persist / load. Both no-ops in this commit.
    bool save (const TrackLibrary& library);
    bool load (TrackLibrary& library);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackStorage)
};
