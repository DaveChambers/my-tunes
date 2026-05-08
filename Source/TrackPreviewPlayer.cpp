#include "TrackPreviewPlayer.h"

TrackPreviewPlayer::TrackPreviewPlayer()
{
    // registerBasicFormats() already pulls in MP3 (via JUCE_USE_MP3AUDIOFORMAT=1)
    // and CoreAudioFormat (.m4a / .aac / .alac / .caf on macOS) for us.
    formatManager.registerBasicFormats();

    const auto err = deviceManager.initialiseWithDefaultDevices (
                         /*numInputChannelsNeeded*/ 0,
                         /*numOutputChannelsNeeded*/ 2);
    if (err.isNotEmpty())
    {
        lastError   = "audio device init: " + err;
        deviceReady = false;
    }
    else
    {
        deviceManager.addAudioCallback (&sourcePlayer);
        sourcePlayer.setSource (&transport);
        transport.addChangeListener (this);
        deviceReady = true;
    }
}

TrackPreviewPlayer::~TrackPreviewPlayer()
{
    transport.removeChangeListener (this);
    transport.stop();
    transport.setSource (nullptr);
    sourcePlayer.setSource (nullptr);
    if (deviceReady)
        deviceManager.removeAudioCallback (&sourcePlayer);
    currentReaderSource.reset();
}

bool TrackPreviewPlayer::play (int libraryTrackIndex, const juce::File& file)
{
    lastError.clear();

    if (! deviceReady)
    {
        lastError = "audio device not ready";
        sendChangeMessage();
        return false;
    }

    if (! file.existsAsFile())
    {
        lastError = "file not found: " + file.getFileName();
        sendChangeMessage();
        return false;
    }

    // Tear down whatever was playing.
    transport.stop();
    transport.setSource (nullptr);
    currentReaderSource.reset();
    playingIndex = -1;
    playingFile  = juce::File();

    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr)
    {
        lastError = "unsupported format: ." + file.getFileExtension().substring (1).toLowerCase();
        sendChangeMessage();
        return false;
    }

    currentReaderSource = std::make_unique<juce::AudioFormatReaderSource> (
                              reader, /*deleteReaderWhenThisIsDeleted*/ true);

    transport.setSource (currentReaderSource.get(),
                         /*readAheadBufferSize*/ 32768,
                         /*readerThread*/        nullptr,
                         /*sourceSampleRateToCorrectFor*/ reader->sampleRate);

    transport.setPosition (0.0);
    transport.start();

    playingIndex = libraryTrackIndex;
    playingFile  = file;

    sendChangeMessage();
    return true;
}

void TrackPreviewPlayer::stop()
{
    if (playingIndex < 0 && ! transport.isPlaying() && currentReaderSource == nullptr)
        return;

    transport.stop();
    transport.setSource (nullptr);
    currentReaderSource.reset();
    playingIndex = -1;
    playingFile  = juce::File();

    sendChangeMessage();
}

void TrackPreviewPlayer::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src == &transport)
    {
        // End of file -> stop + broadcast so UI returns to idle.
        if (transport.hasStreamFinished() && playingIndex >= 0)
        {
            stop();
        }
    }
}

juce::StringArray TrackPreviewPlayer::getSupportedFormatNames() const
{
    juce::StringArray names;
    for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        if (auto* f = formatManager.getKnownFormat (i))
            names.add (f->getFormatName());
    return names;
}
