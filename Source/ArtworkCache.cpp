#include "ArtworkCache.h"

#include "DiscogsClient.h"   // for the User-Agent string

namespace
{
    juce::String guessExtensionFromUrl (const juce::String& url)
    {
        // Best-effort: take the trailing extension of the URL path. We
        // only really care that the stored file ends in something
        // ImageFileFormat recognises.
        const auto trimmed = url.upToFirstOccurrenceOf ("?", false, false);
        const auto lastDot = trimmed.lastIndexOfChar ('.');
        const auto lastSlash = trimmed.lastIndexOfChar ('/');
        if (lastDot > lastSlash && lastDot >= 0)
        {
            const auto ext = trimmed.substring (lastDot + 1).toLowerCase();
            if (ext.length() <= 5
                && (ext == "jpg" || ext == "jpeg" || ext == "png"
                 || ext == "gif" || ext == "webp"))
            {
                return ext;
            }
        }
        return "img"; // fallback — JUCE's ImageFileFormat sniffs by content
    }
}

ArtworkCache::ArtworkCache()  = default;
ArtworkCache::~ArtworkCache() = default;

juce::File ArtworkCache::getCacheFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("My Tunes")
               .getChildFile ("ArtworkCache");
}

juce::File ArtworkCache::getCachedFile (const juce::String& coverUrl) const
{
    if (coverUrl.isEmpty())
        return {};

    // Content-address by URL hash — lower collision risk than
    // String::hashCode64, but JUCE's hashCode is good enough for
    // small sample sizes and avoids pulling SHA in.
    const auto hashed = juce::String::toHexString (coverUrl.hashCode64());
    const auto ext    = guessExtensionFromUrl (coverUrl);
    return getCacheFolder().getChildFile (hashed + "." + ext);
}

juce::File ArtworkCache::ensureCachedFile (const juce::String& url)
{
    if (url.isEmpty())
        return {};

    const auto cached = getCachedFile (url);
    if (cached.existsAsFile() && cached.getSize() > 0)
        return cached;

    auto folder = getCacheFolder();
    if (! folder.exists())
        folder.createDirectory();

    juce::URL u (url);
    const auto opts = juce::URL::InputStreamOptions (
                          juce::URL::ParameterHandling::inAddress)
                          .withConnectionTimeoutMs (10000)
                          .withExtraHeaders ("User-Agent: "
                                              + DiscogsClient::getUserAgent()
                                              + "\r\n");

    auto stream = u.createInputStream (opts);
    if (stream == nullptr)
        return {};

    juce::FileOutputStream out (cached);
    if (! out.openedOk())
        return {};

    out.writeFromInputStream (*stream, -1);
    out.flush();

    if (cached.getSize() <= 0)
    {
        cached.deleteFile();
        return {};
    }

    return cached;
}

juce::Image ArtworkCache::loadCachedImage (const juce::File& file) const
{
    if (! file.existsAsFile())
        return {};
    return juce::ImageCache::getFromFile (file);
}
