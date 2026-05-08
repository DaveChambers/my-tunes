#include "DiscogsClient.h"

DiscogsClient::DiscogsClient()
    : token (getTokenFromEnvironment())
{
}

DiscogsClient::~DiscogsClient() = default;

juce::String DiscogsClient::getTokenFromEnvironment()
{
    // juce::SystemStats::getEnvironmentVariable returns empty if unset.
    return juce::SystemStats::getEnvironmentVariable ("DISCOGS_USER_TOKEN", {});
}

void DiscogsClient::lookup (TrackFile& track)
{
    // Stub: leave status as NotSearched. Real HTTP / JSON parsing lands
    // in the Discogs milestone commit.
    track.discogsStatus = TrackFile::DiscogsStatus::NotSearched;
}
