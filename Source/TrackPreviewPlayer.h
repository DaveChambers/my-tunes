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
    // any previous playback first. Returns true on success; on failure
    // the reason is in getLastError().
    bool play (int libraryTrackIndex, const juce::File& file);

    // Stop + release the current reader. No-op if not playing.
    void stop();

    bool          isPlaying()             const noexcept { return playingIndex >= 0; }
    int           getPlayingTrackIndex()  const noexcept { return playingIndex;      }
    juce::File    getPlayingFile()        const          { return playingFile;       }
    juce::String  getLastError()          const          { return lastError;         }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackPreviewPlayer)
};
