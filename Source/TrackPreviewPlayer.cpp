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
    teardownInternal();
    sourcePlayer.setSource (nullptr);
    if (deviceReady)
        deviceManager.removeAudioCallback (&sourcePlayer);
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

    // ---- Decide where to start --------------------------------------------
    // Resume only when:
    //   - same file as the one we last touched,
    //   - we have a non-zero captured position,
    //   - the previous run didn't end naturally.
    // Anything else is "fresh start from 0".
    const bool sameFile        = (currentTrackFile == file);
    const bool hasResumePoint  = resumePositionSeconds > 0.0;
    const bool canResume       = sameFile && hasResumePoint && ! endedNaturally;

    const double startPositionSeconds = canResume ? resumePositionSeconds : 0.0;

    juce::Logger::writeToLog (
        "Player: play idx=" + juce::String (libraryTrackIndex)
        + " file=\"" + file.getFileName() + "\""
        + (canResume ? juce::String (" resume@") + juce::String (startPositionSeconds, 2) + "s"
                     : juce::String (" from start")));

    // ---- Tear down the old source ----------------------------------------
    teardownInternal();

    // ---- Update identity. -------------------------------------------------
    // If this is a different file, clear the previous resume point
    // because it referred to that other file.
    if (! sameFile)
    {
        resumePositionSeconds = 0.0;
        resumeTotalSeconds    = 0.0;
    }
    currentTrackIndex = libraryTrackIndex;
    currentTrackFile  = file;
    endedNaturally    = false;

    // ---- Open the reader --------------------------------------------------
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
    // setPosition uses the transport's internal sampleRate member,
    // which is only finalised once the audio device's prepareToPlay
    // has run. Seeking the AudioFormatReaderSource directly (in
    // source samples) sidesteps that timing window and guarantees
    // the resume position is honoured even on the very first start()
    // after setSource. We then also call transport.setPosition (in
    // seconds) as the public, resampler-aware seek so the transport's
    // own internal counters match.
    if (startPositionSeconds > 0.0 && reader->sampleRate > 0.0)
    {
        const auto seekSamples = (juce::int64) (startPositionSeconds
                                                * reader->sampleRate);
        currentReaderSource->setNextReadPosition (seekSamples);
    }
    transport.setPosition (startPositionSeconds);

    // The resume position has now been "consumed" — capture happens
    // again only on the next stop()/scrub. Reset to 0 so a subsequent
    // play() of the same file without an intervening stop starts at 0.
    resumePositionSeconds = 0.0;

    // Set the active-playing flag BEFORE transport.start() so that any
    // listener fired synchronously during start() sees consistent
    // state ("we are now playing track N").
    activelyPlaying = true;
    transport.start();

    sendChangeMessage();
    return true;
}

bool TrackPreviewPlayer::resumeFromPause()
{
    if (currentTrackIndex < 0 || currentTrackFile == juce::File() || endedNaturally)
        return false;
    return play (currentTrackIndex, currentTrackFile);
}

void TrackPreviewPlayer::stop()
{
    if (! activelyPlaying)
    {
        // No active playback. We deliberately do NOT clear
        // currentTrackIndex/File here — a previous manual stop has
        // already captured everything we need, and calling stop()
        // twice in a row mustn't wipe the resume point.
        return;
    }

    // Capture position BEFORE tearing down so the transport's read
    // counters are still valid.
    const double pos   = transport.getCurrentPosition();
    const double total = transport.getLengthInSeconds();

    // Treat "within 0.5 s of the end" as a completed playback: next
    // play of this track should start at 0, not at "the last sample".
    const bool atEnd = (total > 0.0 && pos >= total - 0.5);

    resumePositionSeconds = atEnd ? 0.0 : juce::jmax (0.0, pos);
    resumeTotalSeconds    = juce::jmax (0.0, total);

    juce::Logger::writeToLog (
        "Player: stop idx=" + juce::String (currentTrackIndex)
        + " at " + juce::String (pos, 2) + "s / " + juce::String (total, 2) + "s"
        + (atEnd ? juce::String (" (treated as ended)")
                 : juce::String (" -> resume@") + juce::String (resumePositionSeconds, 2) + "s"));

    if (atEnd)
        endedNaturally = true; // suppress hasPausedTrack on UI

    teardownInternal();
    sendChangeMessage();
}

void TrackPreviewPlayer::teardownInternal()
{
    // Clear observable playback state FIRST so any async change-
    // listener callbacks queued by the transport mutations below see
    // activelyPlaying == false and bail before touching transport
    // internals. (JUCE 8.0.4's hasStreamFinished() can deref cleared
    // internals after setSource(nullptr); the existing guard in
    // changeListenerCallback relies on this ordering.)
    activelyPlaying = false;

    transport.stop();
    transport.setSource (nullptr);
    currentReaderSource.reset();
}

void TrackPreviewPlayer::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src != &transport)
        return;

    if (! activelyPlaying)
        return;

    if (! transport.isPlaying())
    {
        // Natural end-of-stream. Next play of this same track should
        // start from 0, not from the last sample.
        juce::Logger::writeToLog (
            "Player: natural end idx=" + juce::String (currentTrackIndex));
        resumePositionSeconds = 0.0;
        resumeTotalSeconds    = 0.0;
        endedNaturally        = true;
        teardownInternal();
        sendChangeMessage();
    }
}

double TrackPreviewPlayer::getCurrentPositionSeconds() const
{
    if (! activelyPlaying || currentReaderSource == nullptr)
        return 0.0;
    return transport.getCurrentPosition();
}

double TrackPreviewPlayer::getTotalLengthSeconds() const
{
    if (! activelyPlaying || currentReaderSource == nullptr)
        return 0.0;
    return transport.getLengthInSeconds();
}

void TrackPreviewPlayer::setPositionSeconds (double seconds)
{
    if (! activelyPlaying || currentReaderSource == nullptr)
        return;

    const double total = transport.getLengthInSeconds();
    const double clamped = juce::jlimit (0.0,
                                          total > 0.0 ? total : seconds,
                                          seconds);
    transport.setPosition (clamped);

    // Scrubbing while playing also updates the resume point. If the
    // user scrubs to 90s then stops, the next play() should resume
    // from 90s — not from wherever the transport was just before the
    // scrub. (This is what Dave's spec calls
    // "scrubber changes should update the position that will be
    // resumed from".)
    resumePositionSeconds = clamped;
    resumeTotalSeconds    = juce::jmax (0.0, total);
}

juce::StringArray TrackPreviewPlayer::getSupportedFormatNames() const
{
    juce::StringArray names;
    for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        if (auto* f = formatManager.getKnownFormat (i))
            names.add (f->getFormatName());
    return names;
}
