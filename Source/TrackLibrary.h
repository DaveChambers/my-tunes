#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "TrackFile.h"

// In-memory collection of scanned tracks.
//
// First-version contract (subject to change once we wire UI events):
//   - holds the most recently scanned folder + the list of tracks we
//     parsed from it
//   - sample-of-20: scanFolder() may keep at most `sampleSize` tracks
class TrackLibrary
{
public:
    TrackLibrary();
    ~TrackLibrary();

    // Scan `folder` for supported audio files, parse filenames, keep
    // up to `sampleSize` tracks. Order is implementation-defined for
    // now (probably folder order).
    void scanFolder (const juce::File& folder, int sampleSize = 20);

    int                            getNumTracks()           const noexcept { return (int) tracks.size(); }
    const std::vector<TrackFile>&  getAllTracks()           const noexcept { return tracks; }
    juce::File                     getFolder()              const          { return folder; }

    const TrackFile& getTrack (int index) const { return tracks[(std::size_t) index]; }
    TrackFile&       getTrack (int index)       { return tracks[(std::size_t) index]; }

private:
    juce::File             folder;
    std::vector<TrackFile> tracks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLibrary)
};
