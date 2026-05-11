#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "../TrackPreviewPlayer.h"

namespace
{
    // Writes a short stereo silent WAV at `file`. Returns true on
    // success. Used to give the resume tests a real, openable audio
    // file without depending on any sample asset shipped with the
    // repo. We pick 6 seconds at 44.1 kHz mono so the file is tiny
    // (~520 KB) yet long enough that "seek to 2.0 s" is a meaningful
    // resume target.
    bool writeTestWav (const juce::File& file)
    {
        const int    sampleRate    = 44100;
        const double durationSecs  = 6.0;
        const int    numChannels   = 1;
        const int    numSamples    = (int) (sampleRate * durationSecs);

        juce::AudioBuffer<float> buf (numChannels, numSamples);
        buf.clear();

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer;

        if (auto out = file.createOutputStream())
        {
            writer.reset (wav.createWriterFor (out.release(),
                                                sampleRate,
                                                (unsigned int) numChannels,
                                                16, {}, 0));
        }
        if (writer == nullptr) return false;

        return writer->writeFromAudioSampleBuffer (buf, 0, numSamples);
    }
}

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

        // ---- Resume contract (Dave's spec). All in a single player
        //      instance to avoid the AudioDeviceManager-reinit hang
        //      noted above. We write two short real WAVs to a temp
        //      folder so play() actually opens a reader and the
        //      seek path runs end-to-end.
        const auto wavA = juce::File::createTempFile (".wav");
        const auto wavB = juce::File::createTempFile (".wav");
        if (! writeTestWav (wavA) || ! writeTestWav (wavB))
        {
            logMessage ("(skipped resume contract tests — couldn't write temp WAVs)");
            wavA.deleteFile();
            wavB.deleteFile();
            return;
        }

        TrackPreviewPlayer p;
        if (p.getLastError().isNotEmpty())
        {
            // Audio device couldn't initialise (sandboxed CI, no audio
            // HAL, etc.) — skip the live tests rather than fail.
            logMessage ("(skipped resume contract tests — audio device unavailable: "
                        + p.getLastError() + ")");
            wavA.deleteFile();
            wavB.deleteFile();
            return;
        }

        beginTest ("play() succeeds on a real WAV and reports activelyPlaying");
        {
            const bool ok = p.play (7, wavA);
            expect       (ok);
            expect       (p.isPlaying());
            expectEquals (p.getPlayingTrackIndex(), 7);
            expectEquals (p.getPlayingFile().getFullPathName(), wavA.getFullPathName());
        }

        beginTest ("manual stop captures the transport position into resume");
        {
            // The audio device may or may not be actually pumping the
            // stream, but transport.getCurrentPosition reflects the
            // next-read position which we can drive directly via
            // setPositionSeconds. Move the playhead to 2.0 s and stop.
            p.setPositionSeconds (2.0);
            p.stop();

            expect       (! p.isPlaying());
            expect       (p.hasPausedTrack());
            expectEquals (p.getPausedTrackIndex(), 7);
            expectEquals (p.getPausedTrackFile().getFullPathName(),
                          wavA.getFullPathName());
            // Allow a tiny tolerance for sample-rounded position.
            expectWithinAbsoluteError (p.getPausedPositionSeconds(),
                                        2.0, 0.05);
        }

        beginTest ("play() of the same file resumes from the captured position");
        {
            const bool ok = p.play (7, wavA);
            expect       (ok);
            expect       (p.isPlaying());
            // After play(), getCurrentPositionSeconds reads the new
            // transport's next-read position. We seeked to 2.0 s in
            // the previous test, so this should be ~2.0 s — NOT 0.
            expectWithinAbsoluteError (p.getCurrentPositionSeconds(),
                                        2.0, 0.1);
        }

        beginTest ("scrub-then-stop captures the SCRUBBED position");
        {
            // We're already playing wavA. Scrub to 4.0 s, then stop.
            p.setPositionSeconds (4.0);
            p.stop();

            expect       (p.hasPausedTrack());
            expectWithinAbsoluteError (p.getPausedPositionSeconds(),
                                        4.0, 0.05);

            // Verify the next play seeks to 4.0 (the scrubbed point),
            // not the previous 2.0.
            const bool ok = p.play (7, wavA);
            expect (ok);
            expectWithinAbsoluteError (p.getCurrentPositionSeconds(),
                                        4.0, 0.1);
        }

        beginTest ("play() of a DIFFERENT file resets resume to 0");
        {
            // Currently playing wavA. Switch to wavB.
            const bool ok = p.play (11, wavB);
            expect       (ok);
            expectEquals (p.getPlayingTrackIndex(), 11);
            // Fresh track => fresh playback.
            expectWithinAbsoluteError (p.getCurrentPositionSeconds(),
                                        0.0, 0.1);

            // And a stop on the new track captures its own position
            // independently.
            p.setPositionSeconds (1.0);
            p.stop();
            expect (p.hasPausedTrack());
            expectEquals (p.getPausedTrackIndex(), 11);
            expectWithinAbsoluteError (p.getPausedPositionSeconds(),
                                        1.0, 0.05);
        }

        beginTest ("hasPausedTrack is false after natural end (simulated by "
                   "seeking past the end)");
        {
            // We're currently paused on wavB at 1.0s. Resume.
            const bool ok = p.play (11, wavB);
            expect (ok);

            // Seek past the end (the WAV is 6 s long), then stop.
            // stop()'s "within 0.5 s of end" rule should treat that
            // as completed and disable hasPausedTrack.
            p.setPositionSeconds (10.0);  // clamps to total length
            p.stop();

            expect (! p.hasPausedTrack(),
                    "stopping at/past end-of-track must not offer a "
                    "stale resume point");
        }

        beginTest ("rapid play/stop/play on real WAV doesn't crash");
        {
            for (int i = 0; i < 6; ++i)
            {
                p.play (12, wavA);
                p.stop();
            }
            expect (! p.isPlaying());
        }

        wavA.deleteFile();
        wavB.deleteFile();
    }
};

static TrackPreviewPlayerTests sTrackPreviewPlayerTests;
