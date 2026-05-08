#pragma once

#include <juce_core/juce_core.h>

#include "TrackFile.h"

// Thin wrapper around the Discogs HTTP API. First version: stubbed.
//
// When implemented:
//   - reads DISCOGS_USER_TOKEN from environment / local config (NEVER
//     committed)
//   - sets User-Agent: "MyTunes/0.1 +https://makeelectronicsound.com"
//   - rate-limits requests (Discogs: 60/min unauth, 60/min auth on
//     identity endpoints; we err on the cautious side)
//   - prefers artist+title search; prefers results with cover images
//   - updates TrackFile::discogsStatus / discogsCoverUrl / etc.
//   - is opt-in: app must run without a token
class DiscogsClient
{
public:
    DiscogsClient();
    ~DiscogsClient();

    // Returns the token from $DISCOGS_USER_TOKEN if present, else empty.
    static juce::String getTokenFromEnvironment();

    // Whether we have a usable auth token at construction time.
    bool hasToken() const noexcept { return token.isNotEmpty(); }

    // Stubbed lookup. Sets `track.discogsStatus = NotSearched` until the
    // real implementation lands.
    void lookup (TrackFile& track);

private:
    juce::String token;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscogsClient)
};
