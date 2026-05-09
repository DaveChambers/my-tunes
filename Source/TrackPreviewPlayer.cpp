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

    // Decide where to start: resume the saved paused position when the
    // track being played matches the one we paused; otherwise from 0.
    double startPositionSeconds = 0.0;
    if (pausedTrackIndex == libraryTrackIndex
        && pausedTrackFile == file
        && pausedPositionSeconds > 0.0)
    {
        startPositionSeconds = pausedPositionSeconds;
    }

    // Either we're consuming the resume position, or the user picked a
    // different track and the saved state is now stale — clear in both
    // cases so the next manual stop captures fresh state.
    clearPausedPosition();

    // Tear down whatever was playing.
    teardownInternal();

    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr)
    {
        lastError = "unsupported format: ." + file.getFileExtension().substring (1).toLowerCase();
        sendChangeMessage();
        return false;
    }

    currentReaderSource = std::make_unique<juce::AudioFormatReaderSource> (
                              reader, /*deleteReaderWhenThisIsDeleted*/ true);

    // See the inline note in the previous fix: read-ahead buffering
    // with a null thread crashes JUCE 8.0.4. Use buffer-size 0.
    transport.setSource (currentReaderSource.get(),
                         /*readAheadBufferSize*/      0,
                         /*readerThread*/             nullptr,
                         /*sourceSampleRateToCorrectFor*/ reader->sampleRate);

    // Belt-and-braces seek for the resume case. AudioTransportSource::
    // setPosition uses the transport's internal sampleRate member, which
    // is only finalised once the audio device's prepareToPlay has run.
    // Seeking the AudioFormatReaderSource directly (in source samples)
    // sidesteps that timing window and guarantees the resume position is
    // honoured even on the very first start() after setSource. We then
    // also call transport.setPosition (in seconds) as the public,
    // resampler-aware seek so internal counters match.
    if (startPositionSeconds > 0.0 && reader->sampleRate > 0.0)
    {
        const auto seekSamples = (juce::int64) (startPositionSeconds
                                                * reader->sampleRate);
        currentReaderSource->setNextReadPosition (seekSamples);
    }
    transport.setPosition (startPositionSeconds);
    transport.start();

    playingIndex = libraryTrackIndex;
    playingFile  = file;

    sendChangeMessage();
    return true;
}

bool TrackPreviewPlayer::resumeFromPause()
{
    if (pausedTrackIndex < 0 || pausedTrackFile == juce::File())
        return false;
    return play (pausedTrackIndex, pausedTrackFile);
}

void TrackPreviewPlayer::stop()
{
    if (playingIndex < 0 && ! transport.isPlaying() && currentReaderSource == nullptr)
        return;

    // Capture the paused position BEFORE tearing down so we can read
    // transport.getCurrentPosition() while the transport still has a
    // valid source.
    capturePausedPosition();

    const int prevIndex = playingIndex;
    teardownInternal();

    if (prevIndex >= 0)
        sendChangeMessage();
}

void TrackPreviewPlayer::capturePausedPosition()
{
    if (playingIndex < 0 || currentReaderSource == nullptr)
        return;

    const double pos   = transport.getCurrentPosition();
    const double total = transport.getLengthInSeconds();

    // If we're effectively at the end (within 0.5 s) treat this as a
    // completed playback rather than a pause — next play of the same
    // track should start from 0, not from "the last sample".
    if (total > 0.0 && pos >= total - 0.5)
    {
        clearPausedPosition();
        return;
    }

    pausedTrackIndex         = playingIndex;
    pausedTrackFile          = playingFile;
    pausedPositionSeconds    = juce::jmax (0.0, pos);
    pausedTotalLengthSeconds = juce::jmax (0.0, total);
}

void TrackPreviewPlayer::clearPausedPosition()
{
    pausedTrackIndex         = -1;
    pausedTrackFile          = juce::File();
    pausedPositionSeconds    = 0.0;
    pausedTotalLengthSeconds = 0.0;
}

void TrackPreviewPlayer::teardownInternal()
{
    // Clear observable state FIRST so any async change-listener
    // callbacks queued by the transport mutations below see
    // playingIndex < 0 and bail before touching transport internals
    // (this is the JUCE 8.0.4 hasStreamFinished() crash path we
    // already fixed; preserving the same ordering here).
    playingIndex = -1;
    playingFile  = juce::File();

    transport.stop();
    transport.setSource (nullptr);
    currentReaderSource.reset();
}

void TrackPreviewPlayer::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src != &transport)
        return;

    if (playingIndex < 0)
        return;

    if (! transport.isPlaying())
    {
        // Natural end-of-stream. The track ran out on its own — clear
        // the paused position so the next play() of this track starts
        // from 0, then tear down.
        clearPausedPosition();
        teardownInternal();
        sendChangeMessage();
    }
}

double TrackPreviewPlayer::getCurrentPositionSeconds() const
{
    if (playingIndex < 0 || currentReaderSource == nullptr)
        return 0.0;
    return transport.getCurrentPosition();
}

double TrackPreviewPlayer::getTotalLengthSeconds() const
{
    if (playingIndex < 0 || currentReaderSource == nullptr)
        return 0.0;
    return transport.getLengthInSeconds();
}

void TrackPreviewPlayer::setPositionSeconds (double seconds)
{
    if (playingIndex < 0 || currentReaderSource == nullptr)
        return;

    const double total = transport.getLengthInSeconds();
    const double clamped = juce::jlimit (0.0,
                                          total > 0.0 ? total : seconds,
                                          seconds);
    transport.setPosition (clamped);
}

juce::StringArray TrackPreviewPlayer::getSupportedFormatNames() const
{
    juce::StringArray names;
    for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        if (auto* f = formatManager.getKnownFormat (i))
            names.add (f->getFormatName());
    return names;
}
