#include "TuneListConfig.h"

juce::File TuneListConfig::getConfigFile()
{
    // On macOS, JUCE's userApplicationDataDirectory maps to plain
    // ~/Library, not ~/Library/Application Support. Compose the
    // standard macOS Application Support path explicitly so the
    // canonical config file location matches what users (and
    // Apple's own docs) expect.
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support")
               .getChildFile ("TuneList")
               .getChildFile ("config.json");
}

juce::String TuneListConfig::env (const char* name)
{
    return juce::SystemStats::getEnvironmentVariable (name, {});
}

TuneListConfig::TuneListConfig()
{
    // 1) Try config.json first. Tolerate parse failures: log a hint
    //    but don't crash, so the rest of the app keeps working.
    juce::var json;
    const auto configFile = getConfigFile();
    if (configFile.existsAsFile())
    {
        const auto text = configFile.loadFileAsString();
        json = juce::JSON::parse (text);
        if (! json.isObject())
        {
            juce::Logger::writeToLog (
                "TuneListConfig: config.json present but invalid JSON: "
                + configFile.getFullPathName());
            json = juce::var();
        }
    }

    auto fromJson = [&] (const char* key) -> juce::String
    {
        if (! json.isObject()) return {};
        const auto v = json.getProperty (key, juce::var());
        return v.isString() ? v.toString() : juce::String();
    };

    auto resolve = [&] (const char* jsonKey,
                        const char* envKey,
                        juce::String& outValue,
                        juce::String& outSource)
    {
        const auto fromConfig = fromJson (jsonKey).trim();
        if (fromConfig.isNotEmpty())
        {
            outValue  = fromConfig;
            outSource = "config.json";
            return;
        }
        const auto fromEnv = env (envKey).trim();
        if (fromEnv.isNotEmpty())
        {
            outValue  = fromEnv;
            outSource = "env";
            return;
        }
        outValue.clear();
        outSource.clear();
    };

    resolve ("spotifyClientId",     "SPOTIFY_CLIENT_ID",
             spotifyClientId,       spotifyClientIdSrc);
    resolve ("spotifyClientSecret", "SPOTIFY_CLIENT_SECRET",
             spotifyClientSecret,   spotifyClientSecretSrc);
    resolve ("discogsUserToken",    "DISCOGS_USER_TOKEN",
             discogsUserToken,      discogsUserTokenSrc);

    // Diagnostic log line — sources only, NEVER the values.
    juce::Logger::writeToLog (
        juce::String ("TuneListConfig: spotifyClientId=")
        + (spotifyClientIdSrc.isEmpty() ? "(missing)" : spotifyClientIdSrc)
        + ", spotifyClientSecret="
        + (spotifyClientSecretSrc.isEmpty() ? "(missing)" : spotifyClientSecretSrc)
        + ", discogsUserToken="
        + (discogsUserTokenSrc.isEmpty() ? "(missing)" : discogsUserTokenSrc)
        + " [config file: " + configFile.getFullPathName()
        + (configFile.existsAsFile() ? " present]" : " not present]"));
}

TuneListConfig::~TuneListConfig() = default;
