#include "SpotifyClient.h"

namespace
{
    constexpr const char* kTokenEndpoint  = "https://accounts.spotify.com/api/token";
    constexpr const char* kSearchEndpoint = "https://api.spotify.com/v1/search";

    // Bearer-token expiry is reported by Spotify in seconds. Refresh a
    // little before the wall-clock expiry to avoid racing requests
    // against a just-expired token mid-batch.
    constexpr juce::int64 kTokenExpiryMarginMs = 30 * 1000;

    juce::String varAsString (const juce::var& v)
    {
        if (v.isString())                                 return v.toString();
        if (v.isInt() || v.isInt64() || v.isDouble())     return v.toString();
        return {};
    }

    // Pick a reasonable-sized album image from a Spotify track's
    // album.images[] array. The API returns descending sizes: 640, 300,
    // 64. We want the medium one (~300px) for the grid card; if only
    // the small one exists we'll take it. We deliberately do not modify
    // or re-encode the file — the cache stores the bytes as Spotify
    // served them, per Spotify's developer policy.
    juce::String pickAlbumImageUrl (const juce::var& images)
    {
        if (! images.isArray() || images.size() == 0)
            return {};

        juce::String chosen;
        int          chosenWidth = -1;

        for (int i = 0; i < images.size(); ++i)
        {
            const auto& img = images[i];
            if (! img.isObject()) continue;
            const auto url = varAsString (img.getProperty ("url", {}));
            const auto w   = (int) img.getProperty ("width", -1);
            if (url.isEmpty()) continue;

            // Prefer something around 300px. If we haven't picked
            // anything yet, take whatever we see first.
            if (chosenWidth < 0)
            {
                chosen      = url;
                chosenWidth = w;
                continue;
            }

            // If the candidate is closer to 300 than the current pick,
            // replace it.
            const int distNew = std::abs (w   - 300);
            const int distOld = std::abs (chosenWidth - 300);
            if (distNew < distOld)
            {
                chosen      = url;
                chosenWidth = w;
            }
        }

        return chosen;
    }

}

SpotifyClient::SpotifyClient()
    : clientId     (getClientIdFromEnvironment()),
      clientSecret (getClientSecretFromEnvironment())
{
}

SpotifyClient::~SpotifyClient() = default;

juce::String SpotifyClient::getClientIdFromEnvironment()
{
    return juce::SystemStats::getEnvironmentVariable ("SPOTIFY_CLIENT_ID", {});
}

juce::String SpotifyClient::getClientSecretFromEnvironment()
{
    return juce::SystemStats::getEnvironmentVariable ("SPOTIFY_CLIENT_SECRET", {});
}

bool SpotifyClient::ensureAccessToken (juce::String& outError)
{
    const auto now = juce::Time::currentTimeMillis();
    if (accessToken.isNotEmpty() && now + kTokenExpiryMarginMs < tokenExpiresAtMs)
        return true;

    return fetchClientCredentialsToken (outError);
}

bool SpotifyClient::fetchClientCredentialsToken (juce::String& outError)
{
    if (! hasCredentials())
    {
        outError = "credentials missing";
        return false;
    }

    juce::Logger::writeToLog ("Spotify: token request started");

    // HTTP Basic auth: base64(client_id:client_secret).
    //
    // SECURITY: clientId / clientSecret are kept in this single
    // MemoryBlock + the resulting Authorization header for the duration
    // of one POST. They are never written to disk, never logged in
    // plaintext, and never exposed through any public accessor on this
    // class.
    const auto basic = clientId + ":" + clientSecret;
    juce::MemoryBlock raw (basic.toRawUTF8(), basic.getNumBytesAsUTF8());
    const auto basicB64 = juce::Base64::toBase64 (raw.getData(), raw.getSize());

    juce::String extraHeaders;
    extraHeaders << "Authorization: Basic " << basicB64 << "\r\n";
    extraHeaders << "Content-Type: application/x-www-form-urlencoded\r\n";

    // POST body: form-encoded grant_type=client_credentials
    juce::URL url (kTokenEndpoint);
    url = url.withPOSTData ("grant_type=client_credentials");

    juce::WebInputStream stream (url, /*isPost*/ true);
    stream.withConnectionTimeout (10000)
          .withExtraHeaders      (extraHeaders);

    if (! stream.connect (nullptr))
    {
        outError = "token: connect failed";
        juce::Logger::writeToLog ("Spotify: token request FAILED (connect)");
        return false;
    }

    const int httpCode = stream.getStatusCode();
    if (httpCode < 200 || httpCode >= 300)
    {
        switch (httpCode)
        {
            case 400: outError = "token HTTP 400 (bad request / grant)"; break;
            case 401: outError = "token HTTP 401 (bad client id/secret)"; break;
            case 403: outError = "token HTTP 403";                       break;
            case 429: outError = "token HTTP 429 (rate-limited)";        break;
            default:  outError = "token HTTP " + juce::String (httpCode); break;
        }
        juce::Logger::writeToLog ("Spotify: token request FAILED HTTP "
                                   + juce::String (httpCode));
        return false;
    }

    const auto body = stream.readEntireStreamAsString();
    const auto json = juce::JSON::parse (body);
    if (! json.isObject())
    {
        outError = "token JSON parse error";
        return false;
    }

    const auto bearer = varAsString (json.getProperty ("access_token", {}));
    const auto expiresInSec = (int) json.getProperty ("expires_in", 0);

    if (bearer.isEmpty())
    {
        outError = "token response missing access_token";
        return false;
    }

    accessToken      = bearer;
    tokenExpiresAtMs = juce::Time::currentTimeMillis()
                        + ((juce::int64) juce::jmax (60, expiresInSec)) * 1000;

    juce::Logger::writeToLog ("Spotify: token OK (expires in "
                               + juce::String (expiresInSec) + "s)");
    return true;
}

SpotifyClient::Result SpotifyClient::lookup (const juce::String& artist,
                                              const juce::String& title)
{
    Result r;
    r.matchedArtist = artist;

    if (! hasCredentials())
    {
        r.status      = TrackFile::SpotifyStatus::CredentialsMissing;
        r.errorReason = "SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET not set";
        juce::Logger::writeToLog ("Spotify: credentials missing - skipping search");
        return r;
    }

    if (artist.isEmpty() && title.isEmpty())
    {
        r.status      = TrackFile::SpotifyStatus::NotSearched;
        r.errorReason = "empty artist + title";
        return r;
    }

    juce::Logger::writeToLog (
        "Spotify: search artist=\"" + artist + "\" title=\"" + title + "\"");

    juce::String tokenError;
    if (! ensureAccessToken (tokenError))
    {
        r.status      = TrackFile::SpotifyStatus::Error;
        r.errorReason = tokenError.isNotEmpty() ? tokenError
                                                 : juce::String ("token error");
        return r;
    }

    // Construct the search query. Spotify's q= supports `artist:"…"`
    // and `track:"…"` field-qualified terms. We let juce::URL escape
    // each parameter value.
    juce::String q;
    if (artist.isNotEmpty()) q << "artist:\"" << artist << "\" ";
    if (title.isNotEmpty())  q << "track:\""  << title  << "\"";
    q = q.trim();

    juce::URL url (kSearchEndpoint);
    url = url.withParameter ("q",     q);
    url = url.withParameter ("type",  "track");
    url = url.withParameter ("limit", "5");

    juce::String extraHeaders;
    extraHeaders << "Authorization: Bearer " << accessToken << "\r\n";
    extraHeaders << "Accept: application/json\r\n";

    juce::WebInputStream stream (url, /*isPost*/ false);
    stream.withConnectionTimeout (10000)
          .withExtraHeaders      (extraHeaders);

    if (! stream.connect (nullptr))
    {
        r.status      = TrackFile::SpotifyStatus::Error;
        r.errorReason = "search: connect failed";
        return r;
    }

    const int httpCode = stream.getStatusCode();

    // Handle a stale-token race: if the API says 401, force-refresh once
    // and retry. (The expiry guard above usually catches this, but the
    // server clock and ours can diverge.)
    if (httpCode == 401)
    {
        accessToken.clear();
        tokenExpiresAtMs = 0;
        juce::String retryErr;
        if (ensureAccessToken (retryErr))
        {
            juce::String retryHeaders;
            retryHeaders << "Authorization: Bearer " << accessToken << "\r\n";
            retryHeaders << "Accept: application/json\r\n";
            juce::WebInputStream retry (url, /*isPost*/ false);
            retry.withConnectionTimeout (10000).withExtraHeaders (retryHeaders);
            if (retry.connect (nullptr) && retry.getStatusCode() >= 200
                && retry.getStatusCode() < 300)
            {
                const auto retryBody = retry.readEntireStreamAsString();
                const auto retryJson = juce::JSON::parse (retryBody);
                if (retryJson.isObject())
                {
                    // Fall through to the parse block by overwriting
                    // the variables we use below.
                    const auto tracks = retryJson.getProperty ("tracks", juce::var());
                    if (! tracks.isObject())
                    {
                        r.status      = TrackFile::SpotifyStatus::Error;
                        r.errorReason = "search JSON unexpected shape";
                        return r;
                    }
                    const auto items = tracks.getProperty ("items", juce::var());
                    if (! items.isArray() || items.size() == 0)
                    {
                        r.status = TrackFile::SpotifyStatus::NotFound;
                        return r;
                    }
                    const auto first = items[0];
                    r.trackId     = varAsString (first.getProperty ("id", {}));
                    r.externalUrl = varAsString (
                                       first.getProperty ("external_urls", juce::var())
                                            .getProperty ("spotify", {}));
                    const auto artistArr = first.getProperty ("artists", juce::var());
                    if (artistArr.isArray() && artistArr.size() > 0)
                        r.matchedArtist = varAsString (artistArr[0].getProperty ("name", {}));
                    r.matchedTitle = varAsString (first.getProperty ("name", {}));
                    const auto album = first.getProperty ("album", juce::var());
                    r.albumId  = varAsString (album.getProperty ("id", {}));
                    r.imageUrl = pickAlbumImageUrl (album.getProperty ("images", juce::var()));

                    r.status = (artist.isNotEmpty()
                                 && r.matchedArtist.containsIgnoreCase (artist))
                                 ? TrackFile::SpotifyStatus::Found
                                 : TrackFile::SpotifyStatus::Uncertain;
                    return r;
                }
            }
        }
        r.status      = TrackFile::SpotifyStatus::Error;
        r.errorReason = "HTTP 401 (token refresh failed)";
        return r;
    }

    if (httpCode < 200 || httpCode >= 300)
    {
        r.status = TrackFile::SpotifyStatus::Error;
        switch (httpCode)
        {
            case 403: r.errorReason = "HTTP 403 Forbidden";       break;
            case 404: r.errorReason = "HTTP 404 Not found";       break;
            case 429: r.errorReason = "HTTP 429 Rate-limited";    break;
            default:  r.errorReason = "HTTP " + juce::String (httpCode); break;
        }
        return r;
    }

    const auto body = stream.readEntireStreamAsString();
    const auto json = juce::JSON::parse (body);
    if (! json.isObject())
    {
        r.status      = TrackFile::SpotifyStatus::Error;
        r.errorReason = "search JSON parse error";
        return r;
    }

    const auto tracks = json.getProperty ("tracks", juce::var());
    if (! tracks.isObject())
    {
        r.status      = TrackFile::SpotifyStatus::Error;
        r.errorReason = "search JSON unexpected shape";
        return r;
    }
    const auto items = tracks.getProperty ("items", juce::var());
    if (! items.isArray() || items.size() == 0)
    {
        r.status = TrackFile::SpotifyStatus::NotFound;
        juce::Logger::writeToLog ("Spotify: 0 results");
        return r;
    }

    juce::Logger::writeToLog ("Spotify: " + juce::String (items.size())
                              + " result(s)");

    // Spotify's relevance ranking is already pretty good; we take the
    // first hit. We could rank by artist-substring match but Dave's
    // sample is small and Spotify's relevance is already good enough.
    const auto first = items[0];

    r.trackId     = varAsString (first.getProperty ("id", {}));
    r.externalUrl = varAsString (
                       first.getProperty ("external_urls", juce::var())
                            .getProperty ("spotify", {}));

    const auto artistArr = first.getProperty ("artists", juce::var());
    if (artistArr.isArray() && artistArr.size() > 0)
        r.matchedArtist = varAsString (artistArr[0].getProperty ("name", {}));

    r.matchedTitle = varAsString (first.getProperty ("name", {}));

    const auto album = first.getProperty ("album", juce::var());
    r.albumId  = varAsString (album.getProperty ("id", {}));
    r.imageUrl = pickAlbumImageUrl (album.getProperty ("images", juce::var()));

    // Found if the matched artist string contains the input artist
    // (case-insensitive), Uncertain otherwise. Mirrors DiscogsClient.
    if (artist.isNotEmpty() && r.matchedArtist.containsIgnoreCase (artist))
        r.status = TrackFile::SpotifyStatus::Found;
    else
        r.status = TrackFile::SpotifyStatus::Uncertain;

    juce::Logger::writeToLog (
        "Spotify: pick \"" + r.matchedArtist + " - " + r.matchedTitle + "\""
        + (r.imageUrl.isEmpty() ? " (no image)" : " (image OK)")
        + " status=" + TrackFile::spotifyStatusText (r.status));

    return r;
}
