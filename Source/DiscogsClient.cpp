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
        if (v.isString())                                 return v.toString();
        if (v.isInt() || v.isInt64() || v.isDouble())     return v.toString();
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
        r.status      = TrackFile::DiscogsStatus::NotSearched;
        r.errorReason = "empty artist + title";
        return r;
    }

    juce::URL url (kSearchEndpoint);
    url = url.withParameter ("type", "release");
    if (artist.isNotEmpty()) url = url.withParameter ("artist", artist);
    if (title.isNotEmpty())  url = url.withParameter ("track",  title);
    url = url.withParameter ("per_page", "5");

    // WebInputStream lets us inspect HTTP status codes — createInputStream
    // would just return null on any non-2xx and we'd lose the diagnostic.
    juce::WebInputStream stream (url, /*isPost*/ false);
    stream.withConnectionTimeout (10000)
          .withExtraHeaders      (makeExtraHeaders (token));

    if (! stream.connect (nullptr))
    {
        r.status      = TrackFile::DiscogsStatus::Error;
        r.errorReason = "network: connect failed";
        return r;
    }

    const int httpCode = stream.getStatusCode();
    if (httpCode < 200 || httpCode >= 300)
    {
        r.status = TrackFile::DiscogsStatus::Error;
        switch (httpCode)
        {
            case 401: r.errorReason = "HTTP 401 Unauthorized (bad token?)"; break;
            case 403: r.errorReason = "HTTP 403 Forbidden";                 break;
            case 404: r.errorReason = "HTTP 404 Not found";                 break;
            case 429: r.errorReason = "HTTP 429 Rate-limited";              break;
            default:  r.errorReason = "HTTP " + juce::String (httpCode);    break;
        }
        return r;
    }

    const auto body = stream.readEntireStreamAsString();
    const auto json = juce::JSON::parse (body);
    if (! json.isObject())
    {
        r.status      = TrackFile::DiscogsStatus::Error;
        r.errorReason = "JSON parse error";
        return r;
    }

    const auto results = json.getProperty ("results", juce::var());
    if (! results.isArray() || results.size() == 0)
    {
        r.status = TrackFile::DiscogsStatus::NotFound;
        return r;
    }

    // Prefer first image-bearing result over a closer text match without art —
    // Dave wants visible cover art.
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
