#include "DiscogsClient.h"

namespace
{
    constexpr const char* kSearchEndpoint = "https://api.discogs.com/database/search";

    juce::String makeExtraHeaders (const juce::String& token)
    {
        juce::String h;
        h << "User-Agent: " << DiscogsClient::getUserAgent() << "\r\n";
        if (token.isNotEmpty())
            h << "Authorization: Discogs token=" << token << "\r\n";
        return h;
    }

    juce::String varAsString (const juce::var& v)
    {
        if (v.isString()) return v.toString();
        if (v.isInt() || v.isInt64() || v.isDouble()) return v.toString();
        return {};
    }
}

DiscogsClient::DiscogsClient()
    : token (getTokenFromEnvironment())
{
}

DiscogsClient::~DiscogsClient() = default;

juce::String DiscogsClient::getUserAgent()
{
    return "MyTunes/0.1 +https://makeelectronicsound.com";
}

juce::String DiscogsClient::getTokenFromEnvironment()
{
    return juce::SystemStats::getEnvironmentVariable ("DISCOGS_USER_TOKEN", {});
}

DiscogsClient::Result DiscogsClient::lookup (const juce::String& artist,
                                              const juce::String& title)
{
    Result r;
    r.matchedArtist = artist;

    if (artist.isEmpty() && title.isEmpty())
    {
        r.status = TrackFile::DiscogsStatus::NotSearched;
        return r;
    }

    juce::URL url (kSearchEndpoint);
    url = url.withParameter ("type", "release");
    if (artist.isNotEmpty()) url = url.withParameter ("artist", artist);
    if (title.isNotEmpty())  url = url.withParameter ("track",  title);
    url = url.withParameter ("per_page", "5");

    const auto opts = juce::URL::InputStreamOptions (
                          juce::URL::ParameterHandling::inAddress)
                          .withConnectionTimeoutMs (10000)
                          .withExtraHeaders (makeExtraHeaders (token));

    auto stream = url.createInputStream (opts);
    if (stream == nullptr)
    {
        r.status = TrackFile::DiscogsStatus::Error;
        return r;
    }

    const auto body = stream->readEntireStreamAsString();
    const auto json = juce::JSON::parse (body);
    if (! json.isObject())
    {
        r.status = TrackFile::DiscogsStatus::Error;
        return r;
    }

    const auto results = json.getProperty ("results", juce::var());
    if (! results.isArray() || results.size() == 0)
    {
        r.status = TrackFile::DiscogsStatus::NotFound;
        return r;
    }

    // Pick the first result that carries a thumbnail when possible —
    // Dave wants visible cover art, so a result without an image is
    // less useful even if it's a closer text match.
    juce::var chosen;
    for (int i = 0; i < results.size(); ++i)
    {
        const auto& candidate = results[i];
        if (varAsString (candidate.getProperty ("thumb",       {})).isNotEmpty()
         || varAsString (candidate.getProperty ("cover_image", {})).isNotEmpty())
        {
            chosen = candidate;
            break;
        }
    }
    if (! chosen.isObject())
        chosen = results[0];

    r.matchedTitle = varAsString (chosen.getProperty ("title", {}));
    r.releaseId    = varAsString (chosen.getProperty ("id",    {}));
    r.resourceUrl  = varAsString (chosen.getProperty ("resource_url", {}));
    r.coverUrl     = varAsString (chosen.getProperty ("cover_image", {}));
    r.thumbUrl     = varAsString (chosen.getProperty ("thumb",       {}));

    // Confidence heuristic: if the artist string appears in the matched
    // result title (Discogs returns "Artist - Album" as a single field),
    // we mark Found; otherwise we mark Uncertain so Dave can spot rough
    // matches in the UI without us misleading the user.
    if (artist.isNotEmpty()
        && r.matchedTitle.containsIgnoreCase (artist))
    {
        r.status = TrackFile::DiscogsStatus::Found;
    }
    else
    {
        r.status = TrackFile::DiscogsStatus::Uncertain;
    }

    return r;
}
