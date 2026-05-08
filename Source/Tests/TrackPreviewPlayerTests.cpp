#include <juce_core/juce_core.h>

#include "../TrackPreviewPlayer.h"

class TrackPreviewPlayerTests : public juce::UnitTest
{
public:
    TrackPreviewPlayerTests() : juce::UnitTest ("TrackPreviewPlayer", "my-tunes") {}

    void runTest() override
    {
        beginTest ("constructed player is idle");
        {
            TrackPreviewPlayer p;
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        beginTest ("registers a usable set of audio formats");
        {
            TrackPreviewPlayer p;
            const auto names = p.getSupportedFormatNames();
            logMessage (juce::String ("formats: ") + names.joinIntoString (", "));

            const auto containsCaseInsensitive = [&] (const juce::String& needle)
            {
                for (const auto& n : names)
                    if (n.containsIgnoreCase (needle))
                        return true;
                return false;
            };

            expect (containsCaseInsensitive ("WAV"),  "missing WAV");
            expect (containsCaseInsensitive ("AIFF"), "missing AIFF");
            // FLAC, MP3, CoreAudio (m4a) are nice-to-have. Don't fail
            // the test if a particular format isn't compiled in — JUCE
            // can be configured without them. WAV+AIFF is the
            // contract this test pins.
        }

        beginTest ("play() with a missing file fails gracefully");
        {
            TrackPreviewPlayer p;
            const bool ok = p.play (0, juce::File ("/no/such/file/here.wav"));
            expect (! ok);
            expect (! p.isPlaying());
            expect (p.getLastError().isNotEmpty(),
                    "lastError should describe the failure");
        }

        beginTest ("stop() is a no-op when nothing is playing");
        {
            TrackPreviewPlayer p;
            p.stop();
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        // (Intentionally not unit-testing live playback here — re-initialising
        //  AudioDeviceManager across multiple TrackPreviewPlayer instances
        //  in the same console process tends to hang on macOS audio HAL
        //  state. The play/stop/switch state machine is exercised
        //  end-to-end when the running app drives the player.)
    }
};

static TrackPreviewPlayerTests sTrackPreviewPlayerTests;
