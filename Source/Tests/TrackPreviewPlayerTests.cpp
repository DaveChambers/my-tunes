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

        beginTest ("play() with default (empty) file fails gracefully");
        {
            TrackPreviewPlayer p;
            const bool ok = p.play (0, juce::File());
            expect (! ok);
            expect (! p.isPlaying());
            expect (p.getLastError().isNotEmpty());
        }

        beginTest ("play() with a negative track index still does not crash");
        {
            // The library bounds check lives in MainComponent::togglePlayback.
            // The player itself just stores whatever index it was given;
            // we still want it to not crash if a misbehaving caller hands
            // it -1 (it'll fail on the missing file, leaving the player
            // idle).
            TrackPreviewPlayer p;
            const bool ok = p.play (-1, juce::File ("/no/such/path.wav"));
            expect       (! ok);
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        beginTest ("repeated failing play() calls do not crash");
        {
            TrackPreviewPlayer p;
            for (int i = 0; i < 5; ++i)
            {
                const bool ok = p.play (i, juce::File ("/nope/" + juce::String (i) + ".wav"));
                expect (! ok);
                expect (! p.isPlaying());
            }
        }

        beginTest ("stop() is a no-op when nothing is playing");
        {
            TrackPreviewPlayer p;
            p.stop();
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        beginTest ("stop() after a failed play() leaves the player idle");
        {
            TrackPreviewPlayer p;
            p.play (3, juce::File ("/missing.wav"));
            p.stop();
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        beginTest ("stop() called twice in a row is safe");
        {
            TrackPreviewPlayer p;
            p.stop();
            p.stop();
            p.stop();
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        beginTest ("rapid play/stop/play cycles do not crash and end idle");
        {
            TrackPreviewPlayer p;
            for (int i = 0; i < 8; ++i)
            {
                p.play (i, juce::File ("/missing-" + juce::String (i) + ".wav"));
                p.stop();
            }
            expect       (! p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), -1);
        }

        beginTest ("listener callback is safe when state has been cleared");
        {
            // Simulate the bug scenario: a queued transport change
            // callback would arrive after stop() has cleared state.
            // Calling our own changeListenerCallback path indirectly
            // via stop() must not crash.
            TrackPreviewPlayer p;
            p.play (0, juce::File ("/missing.wav"));
            p.stop();
            p.stop();   // would have hit hasStreamFinished() after the crash
            expect (! p.isPlaying());
        }

        // (Intentionally not unit-testing live playback here — re-initialising
        //  AudioDeviceManager across multiple TrackPreviewPlayer instances
        //  in the same console process tends to hang on macOS audio HAL
        //  state. The play/stop/switch state machine is exercised
        //  end-to-end when the running app drives the player.)
    }
};

static TrackPreviewPlayerTests sTrackPreviewPlayerTests;
