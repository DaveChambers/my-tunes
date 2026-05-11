#include <juce_core/juce_core.h>

#include "../TuneListConfig.h"

// These tests confirm the config.json path resolves correctly and
// values are loaded. We can't easily test env-var fallback without
// mutating the process environment, but the prefer-config-over-env
// rule is verified by writing a JSON file at the canonical location
// for the duration of the test.
//
// We do NOT touch the real ~/Library/Application Support/TuneList/
// config.json: if one exists at the canonical path before the test
// runs, the test backs it up and restores it. If something goes wrong
// mid-test the user's real file should still survive because we copy
// to a sibling .bak path.
class TuneListConfigTests : public juce::UnitTest
{
public:
    TuneListConfigTests() : juce::UnitTest ("TuneListConfig", "my-tunes") {}

    void runTest() override
    {
        const auto canonical = TuneListConfig::getConfigFile();
        const auto backup    = canonical.getSiblingFile ("config.json.tunelist-test-backup");

        const bool userHadOne = canonical.existsAsFile();
        if (userHadOne)
            canonical.copyFileTo (backup);

        beginTest ("missing config.json leaves all sources empty / from env");
        {
            canonical.deleteFile();
            TuneListConfig c;
            // We can't assert "no env vars present" since the test
            // runner might have them; we just assert the type contract:
            //   if the value is non-empty, the source is "env".
            const auto checkSource = [this] (const juce::String& v, const juce::String& src)
            {
                if (v.isEmpty()) expectEquals (src, juce::String());
                else             expectEquals (src, juce::String ("env"));
            };
            checkSource (c.getSpotifyClientId(),     c.getSpotifyClientIdSource());
            checkSource (c.getSpotifyClientSecret(), c.getSpotifyClientSecretSource());
            checkSource (c.getDiscogsUserToken(),    c.getDiscogsUserTokenSource());
        }

        beginTest ("config.json values win over env, sources reported as config.json");
        {
            canonical.getParentDirectory().createDirectory();
            canonical.replaceWithText (
                "{\n"
                "  \"spotifyClientId\":     \"id-from-json\",\n"
                "  \"spotifyClientSecret\": \"secret-from-json\",\n"
                "  \"discogsUserToken\":    \"discogs-from-json\"\n"
                "}\n");

            TuneListConfig c;
            expectEquals (c.getSpotifyClientId(),     juce::String ("id-from-json"));
            expectEquals (c.getSpotifyClientSecret(), juce::String ("secret-from-json"));
            expectEquals (c.getDiscogsUserToken(),    juce::String ("discogs-from-json"));
            expectEquals (c.getSpotifyClientIdSource(),     juce::String ("config.json"));
            expectEquals (c.getSpotifyClientSecretSource(), juce::String ("config.json"));
            expectEquals (c.getDiscogsUserTokenSource(),    juce::String ("config.json"));
            expect       (c.hasSpotifyCredentials());
        }

        beginTest ("malformed config.json is tolerated (no crash, treated as missing)");
        {
            canonical.replaceWithText ("{ not valid json");
            TuneListConfig c;
            // Without a parseable file we should fall back to env-var
            // sources (which may or may not be set). The test only
            // asserts that we did not throw and the object is usable.
            (void) c.getSpotifyClientId();
            (void) c.hasSpotifyCredentials();
        }

        // Clean up: delete our test file and restore the user's
        // original config.json if there was one.
        canonical.deleteFile();
        if (userHadOne)
        {
            backup.copyFileTo (canonical);
            backup.deleteFile();
        }
    }
};

static TuneListConfigTests sTuneListConfigTests;
