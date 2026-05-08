#include "MiniPlayerComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

MiniPlayerComponent::MiniPlayerComponent (TrackLibrary& l, TrackPreviewPlayer& p)
    : library (l), player (p)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));
    titleLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 13.0f, 0)));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setText ("(no track playing)", juce::dontSendNotification);

    addAndMakeVisible (transportButton);
    transportButton.onClick = [this] { onTransportButton(); };

    addAndMakeVisible (positionSlider);
    positionSlider.setSliderStyle  (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setRange (0.0, 1.0, 0.0);
    positionSlider.setValue (0.0, juce::dontSendNotification);

    positionSlider.onDragStart = [this]
    {
        userIsDragging = true;
    };
    positionSlider.onDragEnd = [this]
    {
        userIsDragging = false;
        if (player.isPlaying())
            player.setPositionSeconds (positionSlider.getValue());
    };

    addAndMakeVisible (timeLabel);
    timeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    timeLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.5f, 0)));
    timeLabel.setJustificationType (juce::Justification::centredRight);

    player.addChangeListener (this);
    library.addChangeListener (this);

    refreshFromPlayer();
    startTimerHz (15);
}

MiniPlayerComponent::~MiniPlayerComponent()
{
    stopTimer();
    library.removeChangeListener (this);
    player.removeChangeListener (this);
}

void MiniPlayerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141519));

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void MiniPlayerComponent::resized()
{
    auto bounds = getLocalBounds().reduced (12, 6);

    transportButton.setBounds (bounds.removeFromLeft (70));
    bounds.removeFromLeft (10);

    timeLabel.setBounds (bounds.removeFromRight (110));
    bounds.removeFromRight (8);

    // Title takes the left third of the remaining strip; slider the rest.
    const int titleWidth = juce::jmin (320, juce::jmax (160, bounds.getWidth() / 3));
    titleLabel.setBounds (bounds.removeFromLeft (titleWidth));
    bounds.removeFromLeft (10);

    positionSlider.setBounds (bounds);
}

void MiniPlayerComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromPlayer();
}

void MiniPlayerComponent::timerCallback()
{
    if (! player.isPlaying() || userIsDragging)
        return;

    const double pos    = player.getCurrentPositionSeconds();
    const double total  = player.getTotalLengthSeconds();

    if (total > 0.0)
    {
        // Slider values silently clamp to [min,max]. Suppress callbacks
        // so we don't loop through onValueChange while syncing the UI.
        positionSlider.setValue (pos, juce::dontSendNotification);
    }

    timeLabel.setText (formatTime (pos) + " / " + formatTime (total),
                       juce::dontSendNotification);
}

void MiniPlayerComponent::onTransportButton()
{
    // The button only makes sense while a track is playing — we use it
    // as a "stop" affordance. (Resume-from-paused is out of scope; once
    // stop fires, the source is released and there's nothing to resume.)
    if (player.isPlaying())
        player.stop();
}

void MiniPlayerComponent::refreshFromPlayer()
{
    const bool playing = player.isPlaying();

    if (playing)
    {
        const int idx = player.getPlayingTrackIndex();
        juce::String text = "Playing";
        if (idx >= 0 && idx < library.getNumTracks())
        {
            const auto& tf = library.getTrack (idx);
            text = tf.artist
                    + (tf.title.isNotEmpty()
                          ? juce::String (" - ") + tf.title
                          : juce::String());
        }
        titleLabel.setText (text, juce::dontSendNotification);

        const double total = player.getTotalLengthSeconds();
        positionSlider.setRange (0.0, total > 0.0 ? total : 1.0, 0.0);
        const double pos = player.getCurrentPositionSeconds();
        if (! userIsDragging)
            positionSlider.setValue (pos, juce::dontSendNotification);
        timeLabel.setText (formatTime (pos) + " / " + formatTime (total),
                           juce::dontSendNotification);
    }
    else
    {
        titleLabel.setText ("(no track playing)", juce::dontSendNotification);
        positionSlider.setValue (0.0, juce::dontSendNotification);
        timeLabel.setText ("0:00 / 0:00", juce::dontSendNotification);
    }

    updateLayoutEnabled (playing);
}

void MiniPlayerComponent::updateLayoutEnabled (bool playing)
{
    transportButton.setEnabled (playing);
    positionSlider.setEnabled  (playing);
    transportButton.setButtonText (playing ? "Stop" : "Play");
    repaint();
}

juce::String MiniPlayerComponent::formatTime (double seconds)
{
    if (seconds < 0 || ! std::isfinite (seconds)) seconds = 0;
    const int total = (int) seconds;
    const int m = total / 60;
    const int s = total % 60;
    return juce::String (m) + ":" + juce::String (s).paddedLeft ('0', 2);
}
