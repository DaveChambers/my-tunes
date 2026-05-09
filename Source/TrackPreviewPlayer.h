#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <memory>

// Single-track audio preview player.
//
// Owns the audio device + format stack and exposes a tiny play/stop
// API. Only one track plays at a time — calling play() on a second
// track stops the first cleanly.
//
// Threading:
//   - play() / stop() are called from the message thread.
//   - The audio thread reads through AudioTransportSource +
//     AudioFormatReaderSource (managed inside JUCE).
//   - End-of-file is detected via the transport's ChangeBroadcaster:
//     this class listens, calls stop() internally, and re-broadcasts.
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
    // track index so the UI can ask "is N currently playing?". Stops
    // any previous playback first.
    //
    // Resume contract:
    //   - if (libraryTrackIndex, file) matches the most recently
    //     manually-stopped track and there's a remembered position,
    //     play() resumes from that position;
    //   - otherwise play() starts from 0;
    //   - in both cases the remembered position is consumed (cleared)
    //     after the resume so the next stop/play cycle is fresh.
    //
    // Returns true on success; on failure see getLastError().
    bool play (int libraryTrackIndex, const juce::File& file);

    // Manual stop. Captures the current playback position into the
    // "paused" slot before tearing down the audio source — that's what
    // makes a subsequent play() of the same track resume mid-track
    // instead of starting from zero. No-op if nothing is playing.
    void stop();

    // Resume the most recently paused track from its saved position.
    // Returns false if there is no paused track. Used by the bottom
    // mini-player so its single button acts as pause/resume.
    bool resumeFromPause();

    bool          isPlaying()             const noexcept { return playingIndex >= 0; }
    int           getPlayingTrackIndex()  const noexcept { return playingIndex;      }
    juce::File    getPlayingFile()        const          { return playingFile;       }
    juce::String  getLastError()          const          { return lastError;         }

    // Paused-track accessors (the "stop captured position" state).
    bool          hasPausedTrack()        const noexcept { return pausedTrackIndex >= 0; }
    int           getPausedTrackIndex()   const noexcept { return pausedTrackIndex; }
    juce::File    getPausedTrackFile()    const          { return pausedTrackFile;  }
    double        getPausedPositionSeconds() const noexcept { return pausedPositionSeconds; }
    double        getPausedLengthSeconds()   const noexcept { return pausedTotalLengthSeconds; }

    // Transport accessors. All three are safe when nothing is loaded
    // (return 0.0). Bounds for setPosition are clamped to the loaded
    // file's length.
    double        getCurrentPositionSeconds() const;
    double        getTotalLengthSeconds()     const;
    void          setPositionSeconds (double seconds);

    // For unit tests / diagnostics.
    juce::StringArray getSupportedFormatNames() const;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::AudioDeviceManager   deviceManager;
    juce::AudioFormatManager   formatManager;
    juce::AudioSourcePlayer    sourcePlayer;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> currentReaderSource;

    int          playingIndex = -1;
    juce::File   playingFile;
    juce::String lastError;
    bool         deviceReady  = false;

    // Resume / pause state. Only set when stop() is called by the
    // user while transport is mid-track. Cleared on play() of any
    // track, on natural end-of-file, and on destruction.
    int          pausedTrackIndex          = -1;
    juce::File   pausedTrackFile;
    double       pausedPositionSeconds     = 0.0;
    double       pausedTotalLengthSeconds  = 0.0;

    void capturePausedPosition();
    void clearPausedPosition();
    void teardownInternal();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackPreviewPlayer)
};
