#include "TrackLibrary.h"

#include "TrackFilenameParser.h"

TrackLibrary::TrackLibrary()  = default;
TrackLibrary::~TrackLibrary() = default;

void TrackLibrary::scanFolder (const juce::File& f, int /*sampleSize*/)
{
    folder = f;
    tracks.clear();
    // Real implementation lands in the next commit (folder traversal
    // + extension filter + sample-of-20). Kept as a stub here so the
    // skeleton commit builds cleanly.
}
