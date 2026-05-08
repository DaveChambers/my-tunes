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

    // CRITICAL: in JUCE 8 (incl. 8.0.4) AudioTransportSource::setSource
    // takes (source, readAheadBufferSize, readerThread, ...). If
    // readAheadBufferSize > 0 it constructs an internal
    // BufferingAudioSource using `*readerThread` — passing a null
    // thread there dereferences null and crashes the message thread on
    // the next setNextReadPosition()/start() (we hit this exactly:
    // pthread_mutex_lock on 0x218 inside TimeSliceThread::moveToFrontOfQueue).
    //
    // Disable read-ahead buffering by passing 0 here. For 20 short
    // local files this is fine — the audio thread reads directly from
    // the AudioFormatReaderSource. If we later want pre-buffering,
    // TrackPreviewPlayer must own a long-lived juce::TimeSliceThread
    // member, started in the ctor and stopped in the dtor, and pass
    // its address here.
    transport.setSource (currentReaderSource.get(),
                         /*readAheadBufferSize*/      0,
                         /*readerThread*/             nullptr,
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

    const int prevIndex = playingIndex;

    // Clear our observable state FIRST. Any async change-listener
    // callbacks queued by the transport mutations below will then see
    // playingIndex < 0 and bail out before touching transport
    // internals — which is the path that crashed in JUCE 8.0.4 when
    // we accessed hasStreamFinished() after setSource(nullptr).
    playingIndex = -1;
    playingFile  = juce::File();

    transport.stop();
    transport.setSource (nullptr);
    currentReaderSource.reset();

    if (prevIndex >= 0)
        sendChangeMessage();
}

void TrackPreviewPlayer::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src != &transport)
        return;

    // We've already torn down (either via our own stop(), or via
    // play() switching to a different track that's already updated
    // playingIndex). The queued transport callback doesn't tell us
    // anything new, and probing transport accessors right after
    // setSource(nullptr) hit the bug we're fixing.
    if (playingIndex < 0)
        return;

    // Natural end-of-stream: transport stops on its own when EOF is
    // reached on the audio thread.
    //
    // We deliberately do NOT call transport.hasStreamFinished() here.
    // In JUCE 8.0.4 that accessor walks into transport-internal source
    // state which is unsafe to touch on the heels of a setSource()
    // change — Dave's stop-crash trace landed exactly there
    // (EXC_BAD_ACCESS at 0x0, inside hasStreamFinished()).
    // transport.isPlaying() reads a plain bool flag and is safe.
    if (! transport.isPlaying())
        stop();
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
