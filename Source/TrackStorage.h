#pragma once

#include <juce_core/juce_core.h>

#include <unordered_map>

#include "TrackFile.h"

class TrackLibrary;

// JSON-backed persistence for the user-applied + Discogs-derived fields
// on each track. Lives at:
//
//   ~/Library/Application Support/My Tunes/my-tunes-library.json
//
// Identity: tracks are keyed by full file path. That's good enough for
// v0.1 — a moved file simply gets fresh state next scan, which Dave can
// accept while the app is single-user / single-folder.
//
// Lifecycle from MainComponent:
//   1. ctor: load() reads JSON into an in-memory map.
//   2. after each TrackLibrary::scanFolder, applySavedFieldsToLibrary()
//      walks the freshly-scanned tracks and re-applies stored tags /
//      priority / notes / Discogs metadata.
//   3. on every library change broadcast, snapshotAndSave() writes the
//      current library state back to disk.
class TrackStorage
{
public:
    TrackStorage();
    explicit TrackStorage (juce::File customStorageFile); // for tests
    ~TrackStorage();

    // Where the JSON lives. Created lazily on first save.
    static juce::File getLibraryFile();

    // Effective path used by load/save: the custom file if provided,
    // else getLibraryFile().
    juce::File getActiveFile() const;

    // Load JSON into the in-memory map. Returns true if the file was
    // present and parsed cleanly. False if it doesn't exist yet, can't
    // be opened, or contains invalid JSON.
    bool load();

    // Walk `library.tracks` and apply previously-saved fields onto any
    // track whose full path matches a saved entry. Doesn't broadcast.
    // Calling code (MainComponent) is responsible for triggering a UI
    // refresh after this if needed.
    void applySavedFieldsToLibrary (TrackLibrary& library) const;

    // Replace the in-memory map with the current library state and
    // write it to disk. Returns true on success.
    bool snapshotAndSave (const TrackLibrary& library);

    // Path used by snapshotAndSave for the most recently scanned folder.
    juce::String getSavedFolderPath() const noexcept { return savedFolderPath; }

private:
    struct SavedTrack
    {
        juce::StringArray emojiTags;
        int               priority = 0;
        juce::String      notes;

        TrackFile::DiscogsStatus discogsStatus = TrackFile::DiscogsStatus::NotSearched;
        juce::String  discogsArtist;
        juce::String  discogsTitle;
        juce::String  discogsReleaseId;
        juce::String  discogsCoverUrl;
        juce::String  discogsThumbUrl;
        juce::String  discogsErrorReason;
        juce::String  cachedArtworkPath;
    };

    // Keyed by full file path.
    std::unordered_map<juce::String, SavedTrack> byPath;
    juce::String                                  savedFolderPath;
    juce::File                                    customFile; // empty -> default location

    static juce::String discogsStatusToString (TrackFile::DiscogsStatus);
    static TrackFile::DiscogsStatus discogsStatusFromString (const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackStorage)
};
