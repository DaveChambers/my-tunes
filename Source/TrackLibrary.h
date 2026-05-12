#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <vector>

#include "TrackFile.h"

// In-memory collection of scanned tracks. ChangeBroadcaster: anything
// that mutates the library's tracks (scan, tag toggle, future priority/
// notes edits) calls sendChangeMessage(); listeners refresh.
//
// Contract:
//   - holds the most recently scanned folder + the parsed tracks
//   - scanFolder() loads every supported file by default. The
//     `sampleSize` parameter is kept as a soft cap for tests; pass
//     0 or omit it to scan everything.
class TrackLibrary : public juce::ChangeBroadcaster
{
public:
    TrackLibrary();
    ~TrackLibrary() override;

    // Scan `folder` for supported audio files, parse filenames, and
    // load every matching file. Broadcasts on completion. If
    // `sampleSize` > 0, scanning stops after that many tracks
    // (mainly used by tests to keep fixtures bounded).
    void scanFolder (const juce::File& folder, int sampleSize = 0);

    int                            getNumTracks()           const noexcept { return (int) tracks.size(); }
    const std::vector<TrackFile>&  getAllTracks()           const noexcept { return tracks; }
    juce::File                     getFolder()              const          { return folder; }

    const TrackFile& getTrack (int index) const { return tracks[(std::size_t) index]; }
    TrackFile&       getTrack (int index)       { return tracks[(std::size_t) index]; }

    // Toggle `emoji` on the track at `index`. If the tag is already
    // present, removes it; otherwise adds it. Broadcasts on change.
    // Returns true if the tag is set after the toggle, false if it was
    // removed (or if the index was out of range, in which case nothing
    // changes).
    bool toggleEmojiTag (int trackIndex, const juce::String& emoji);

    // Number of tracks currently carrying `emoji`. Used to update the
    // count next to each board in the left sidebar.
    int countTracksWithTag (const juce::String& emoji) const;

    // Append a track directly (no file scan). Used by TrackStorage::load
    // (future) and by unit tests. Broadcasts.
    void appendTrack (TrackFile track);

private:
    juce::File             folder;
    std::vector<TrackFile> tracks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLibrary)
};
