#include "MetadataProxyClient.h"

namespace
{
    juce::String varAsString (const juce::var& v)
    {
        if (v.isString())                                 return v.toString();
        if (v.isInt() || v.isInt64() || v.isDouble())     return v.toString();
        return {};
    }

    MetadataProxyClient::Source parseSource (const juce::String& s)
    {
        const auto lower = s.toLowerCase();
        if (lower == "spotify") return MetadataProxyClient::Source::Spotify;
        if (lower == "discogs") return MetadataProxyClient::Source::Discogs;
        if (lower == "error")   return MetadataProxyClient::Source::Error;
        return MetadataProxyClient::Source::None;
    }
}

MetadataProxyClient::MetadataProxyClient (juce::String endpointUrl)
    : endpoint (std::move (endpointUrl))
{
}

MetadataProxyClient::~MetadataProxyClient() = default;

juce::String MetadataProxyClient::getDefaultEndpoint()
{
    // Pinned to the Cloud Run deployment. Has the Spotify + Discogs
    // credentials in Secret Manager and implements the
    // Spotify-first / Discogs-fallback policy itself, so the macOS
    // app no longer needs any local API tokens for normal use.
    return "https://tunelist-metadata-96674523622.europe-north1.run.app/lookup";
}

MetadataProxyClient::Result
MetadataProxyClient::lookup (const juce::String& artist,
                              const juce::String& title)
{
    Result r;
    r.artist = artist;
    r.title  = title;

    if (artist.isEmpty() && title.isEmpty())
    {
        r.source      = Source::None;
        r.errorReason = "empty artist + title";
        return r;
    }

    juce::URL url (endpoint);
    if (artist.isNotEmpty()) url = url.withParameter ("artist", artist);
    if (title.isNotEmpty())  url = url.withParameter ("title",  title);

    juce::Logger::writeToLog (
        "Metadata: GET " + endpoint
        + " artist=\"" + artist + "\" title=\"" + title + "\"");

    juce::String extraHeaders;
    extraHeaders << "Accept: application/json\r\n";

    juce::WebInputStream stream (url, /*isPost*/ false);
    stream.withConnectionTimeout (15000)
          .withExtraHeaders      (extraHeaders);

    if (! stream.connect (nullptr))
    {
        r.source      = Source::Error;
        r.errorReason = "metadata: connect failed";
        juce::Logger::writeToLog ("Metadata: connect FAILED");
        return r;
    }

    r.httpCode = stream.getStatusCode();
    if (r.httpCode < 200 || r.httpCode >= 300)
    {
        r.source      = Source::Error;
        r.errorReason = "metadata HTTP " + juce::String (r.httpCode);
        juce::Logger::writeToLog (
            "Metadata: HTTP " + juce::String (r.httpCode) + " FAILED");
        return r;
    }

    const auto body = stream.readEntireStreamAsString();
    auto parsed = parseResponseJson (body);
    // Preserve the live HTTP code + fall back to the input query
    // fields if the proxy didn't echo them.
    parsed.httpCode = r.httpCode;
    if (parsed.artist.isEmpty()) parsed.artist = artist;
    if (parsed.title.isEmpty())  parsed.title  = title;

    juce::Logger::writeToLog (
        juce::String ("Metadata: source=")
        + (parsed.source == Source::Spotify ? "spotify"
           : parsed.source == Source::Discogs ? "discogs"
           : parsed.source == Source::Error   ? "error"
                                                : "none")
        + " artist=\"" + parsed.artist + "\" title=\"" + parsed.title
        + "\" album=\"" + parsed.album + "\""
        + (parsed.imageUrl.isNotEmpty() ? juce::String (" (image OK)")
                                          : juce::String (" (no image)")));

    return parsed;
}

MetadataProxyClient::Result
MetadataProxyClient::parseResponseJson (const juce::String& body)
{
    Result r;

    const auto json = juce::JSON::parse (body);
    if (! json.isObject())
    {
        r.source      = Source::Error;
        r.errorReason = "metadata JSON parse error";
        return r;
    }

    r.source             = parseSource (varAsString (json.getProperty ("source", {})));
    r.artist             = varAsString (json.getProperty ("artist", {}));
    r.title              = varAsString (json.getProperty ("title",  {}));
    r.album              = varAsString (json.getProperty ("album",  {}));
    r.imageUrl           = varAsString (json.getProperty ("imageUrl", {}));
    r.spotifyTrackId     = varAsString (json.getProperty ("spotifyTrackId", {}));
    r.spotifyExternalUrl = varAsString (json.getProperty ("spotifyUrl",     {}));
    r.discogsReleaseId   = varAsString (json.getProperty ("discogsId",      {}));
    r.discogsExternalUrl = varAsString (json.getProperty ("discogsUrl",     {}));
    r.query              = varAsString (json.getProperty ("query", {}));

    if (r.source == Source::Error)
    {
        r.errorReason = varAsString (json.getProperty ("error", {}));
        if (r.errorReason.isEmpty())
            r.errorReason = "backend reported error";
    }

    return r;
}
