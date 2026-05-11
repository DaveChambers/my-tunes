#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <memory>

// Single-track audio preview player.
//
// State model (Dave's spec):
//   - The player remembers which track is "loaded" (currentTrackIndex
//     + currentTrackFile) across a manual stop. Manual stop does NOT
//     clear that identity — it just captures the transport's current
//     position into resumePositionSeconds.
//   - A subsequent play() of the SAME file resumes from
//     resumePositionSeconds. A play() of a DIFFERENT file resets the
//     resume position to 0 (and updates the identity).
//   - Scrubbing while playing also updates resumePositionSeconds, so
//     scrub-then-stop-then-play resumes from the scrubbed position.
//   - When the track reaches natural end-of-stream the player clears
//     resumePositionSeconds (to 0) and latches endedNaturally — the
//     next play() of any track starts from 0. The mini-player's
//     resume button is disabled in that state until the user picks a
//     track explicitly.
//
// Threading:
//   - play() / stop() / setPositionSeconds() are called from the
//     message thread only.
//   - The audio thread reads through AudioTransportSource +
//     AudioFormatReaderSource (managed inside JUCE).
//   - End-of-file is detected via the transport's ChangeBroadcaster;
//     this class listens, updates state on the message thread, then
//     re-broadcasts.
//
// Format support is registered at construction:
//   - WAV / AIFF / FLAC / OGG     via registerBasicFormats()
//   - MP3                          via JUCE_USE_MP3AUDIOFORMAT=1
//   - M4A / AAC / ALAC / CAF       via CoreAudioFormat (macOS)
class TrackPreviewPlayer : public juce::ChangeBroadcaster,
                            private juce::ChangeListener
{
public:
    TrackPreviewPlayer();
    ~TrackPreviewPlayer() override;

    // Start playback of `file`, tagging the playback with the library
    // track index so the UI can ask "is N currently playing?".
    //
    // Resume contract:
    //   - if `file` matches the currentTrackFile AND we have a non-
    //     zero resumePositionSeconds AND the previous playback did
    //     not end naturally, play() seeks to resumePositionSeconds
    //     before starting;
    //   - otherwise play() seeks to 0;
    //   - resumePositionSeconds is then set to 0 for the new play —
    //     it gets re-captured the next time the user manually stops
    //     or scrubs.
    //
    // Returns true on success; on failure see getLastError().
    bool play (int libraryTrackIndex, const juce::File& file);

    // Manual stop. Captures the current playback position into
    // resumePositionSeconds before tearing down the audio source, so
    // a subsequent play() of the SAME file resumes mid-track. Track
    // identity (currentTrackIndex/File) is preserved. No-op if
    // nothing is actively playing.
    void stop();

    // Resume the most recently active track from its captured
    // position. Returns false if there is no track to resume from
    // (e.g. fully idle, or the previous track ended naturally).
    bool resumeFromPause();

    // True between a successful play() and any stop (manual or
    // natural). Decoupled from track identity so the player can
    // remember "what track did we last touch?" even when not
    // actively outputting audio.
    bool          isPlaying()             const noexcept { return activelyPlaying; }

    // Track index that's actively producing audio right now. -1 when
    // not playing (even if a track is loaded and pause-resumable).
    int           getPlayingTrackIndex()  const noexcept
                    { return activelyPlaying ? currentTrackIndex : -1; }
    juce::File    getPlayingFile()        const
                    { return activelyPlaying ? currentTrackFile : juce::File(); }
    juce::String  getLastError()          const          { return lastError;     }

    // Resume / pause query:
    // hasPausedTrack() is true when we manually stopped (or scrubbed
    // and stopped) and the track is ready to resume from a non-zero
    // position. False after natural end or when fully idle.
    bool          hasPausedTrack()        const noexcept
    {
        return ! activelyPlaying
            && currentTrackIndex >= 0
            && currentTrackFile != juce::File()
            && ! endedNaturally;
    }
    int           getPausedTrackIndex()   const noexcept { return currentTrackIndex; }
    juce::File    getPausedTrackFile()    const          { return currentTrackFile;  }
    double        getPausedPositionSeconds() const noexcept { return resumePositionSeconds; }
    double        getPausedLengthSeconds()   const noexcept { return resumeTotalSeconds; }

    // Transport accessors. All three are safe when nothing is loaded
    // (return 0.0). setPositionSeconds while playing also updates the
    // resume point so a subsequent stop+play picks up at the scrubbed
    // position instead of where we were before the scrub.
    double        getCurrentPositionSeconds() const;
    double        getTotalLengthSeconds()     const;
    void          setPositionSeconds (double seconds);

    // For unit tests / diagnostics.
    juce::StringArray getSupportedFormatNames() const;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void teardownInternal();

    juce::AudioDeviceManager   deviceManager;
    juce::AudioFormatManager   formatManager;
    juce::AudioSourcePlayer    sourcePlayer;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> currentReaderSource;

    // Identity of the track we're "on". Preserved across manual
    // stop. Cleared only on destruction; replaced on any play() with
    // a different file.
    int          currentTrackIndex = -1;
    juce::File   currentTrackFile;

    // True between a successful play() and the matching stop /
    // natural-end / new-play.
    bool         activelyPlaying = false;

    // Where to start from on the next play() of currentTrackFile.
    // 0 means "start from beginning". Updated by stop(),
    // setPositionSeconds (while playing), reset to 0 on natural end
    // and on play() of a different file.
    double       resumePositionSeconds = 0.0;
    double       resumeTotalSeconds    = 0.0;

    // Latched when transport hits end-of-stream on its own. Cleared
    // by a fresh play(). Used to gate hasPausedTrack() so the
    // mini-player doesn't offer to "resume" a track that just
    // finished.
    bool         endedNaturally = false;

    juce::String lastError;
    bool         deviceReady  = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackPreviewPlayer)
};
