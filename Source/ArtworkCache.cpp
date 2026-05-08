#include "ArtworkCache.h"

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

    // Use a content-addressed name so cached files survive across runs.
    const juce::String hash = juce::String::toHexString (coverUrl.hashCode());
    return getCacheFolder().getChildFile (hash + ".jpg");
}

juce::Image ArtworkCache::getCachedImage (const juce::String&) const
{
    // Real implementation in the Discogs milestone.
    return juce::Image();
}
