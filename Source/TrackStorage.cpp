#include "TrackStorage.h"

#include "TrackLibrary.h"

TrackStorage::TrackStorage()  = default;
TrackStorage::~TrackStorage() = default;

juce::File TrackStorage::getLibraryFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("My Tunes")
               .getChildFile ("my-tunes-library.json");
}

bool TrackStorage::save (const TrackLibrary&)
{
    // Real implementation in the next-but-one commit.
    return false;
}

bool TrackStorage::load (TrackLibrary&)
{
    return false;
}
