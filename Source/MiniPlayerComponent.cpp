#include "MiniPlayerComponent.h"

#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

// =============================================================================
// TransportIconButton — paints a chunky play triangle or stop square.
//                       No text. Black-and-white geometric.
// =============================================================================

MiniPlayerComponent::TransportIconButton::TransportIconButton()
    : juce::Button ({})
{
    // No tooltip / no text — the shape is the affordance.
}

void MiniPlayerComponent::TransportIconButton::setShowingStop (bool s)
{
    if (showingStop == s) return;
    showingStop = s;
    repaint();
}

void MiniPlayerComponent::TransportIconButton::paintButton (juce::Graphics& g,
                                                             bool isOver,
                                                             bool isDown)
{
    const auto fb = getLocalBounds().toFloat();
    const float cx = fb.getCentreX();
    const float cy = fb.getCentreY();
    const float r  = juce::jmin (fb.getWidth(), fb.getHeight()) * 0.42f;

    const juce::Colour discNormal (0xff262830);
    const juce::Colour discOver   (0xff2c2f38);
    const juce::Colour discDown   (0xff20222a);

    g.setColour (isDown ? discDown : (isOver ? discOver : discNormal));
    g.fillEllipse (fb.reduced (1.0f));

    g.setColour (juce::Colour (0xff34373f));
    g.drawEllipse (fb.reduced (1.5f), 1.0f);

    const auto glyphColour = isEnabled() ? juce::Colours::white
                                          : juce::Colours::grey.withAlpha (0.5f);
    g.setColour (glyphColour);

    if (showingStop)
    {
        // Chunky filled square
        const float side = r * 1.35f;
        g.fillRect (juce::Rectangle<float> (cx - side / 2,
                                              cy - side / 2,
                                              side, side));
    }
    else
    {
        // Chunky right-pointing triangle (slightly inset on the left
        // so the triangle looks visually centred inside the disc).
        juce::Path p;
        p.startNewSubPath (cx - r * 0.55f, cy - r);
        p.lineTo          (cx - r * 0.55f, cy + r);
        p.lineTo          (cx + r,         cy);
        p.closeSubPath();
        g.fillPath (p);
    }
}

// =============================================================================
// MiniPlayerComponent
// =============================================================================

MiniPlayerComponent::MiniPlayerComponent (TrackLibrary& l, TrackPreviewPlayer& p)
    : library (l), player (p)
{
    addAndMakeVisible (positionSlider);
    positionSlider.setSliderStyle  (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setRange (0.0, 1.0, 0.0);
    positionSlider.setValue (0.0, juce::dontSendNotification);

    positionSlider.onDragStart = [this] { userIsDragging = true;  };
    positionSlider.onDragEnd   = [this]
    {
        userIsDragging = false;
        if (player.isPlaying())
            player.setPositionSeconds (positionSlider.getValue());
    };

    addAndMakeVisible (transportButton);
    transportButton.onClick = [this] { onTransportButton(); };

    auto setupTimeLabel = [] (juce::Label& l, juce::Justification j)
    {
        l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        l.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
        l.setJustificationType (j);
    };
    setupTimeLabel (elapsedLabel,   juce::Justification::centredLeft);
    setupTimeLabel (remainingLabel, juce::Justification::centredRight);

    addAndMakeVisible (elapsedLabel);
    addAndMakeVisible (remainingLabel);

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
    // Two-row layout:
    //   row 1: scrub bar full width
    //   row 2: elapsed (left) | transport button (centred) | remaining (right)
    auto bounds = getLocalBounds().reduced (12, 6);

    auto scrubRow  = bounds.removeFromTop (juce::jmax (18, bounds.getHeight() / 2));
    positionSlider.setBounds (scrubRow);

    auto buttonRow = bounds;
    const int btnSize = juce::jmax (28, juce::jmin (40, buttonRow.getHeight()));
    const int timeWidth = 56;

    elapsedLabel  .setBounds (buttonRow.removeFromLeft  (timeWidth));
    remainingLabel.setBounds (buttonRow.removeFromRight (timeWidth));

    auto btnRect = juce::Rectangle<int> (0, 0, btnSize, btnSize)
                       .withCentre (buttonRow.getCentre());
    transportButton.setBounds (btnRect);
}

void MiniPlayerComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromPlayer();
}

void MiniPlayerComponent::timerCallback()
{
    if (! player.isPlaying() || userIsDragging)
        return;

    const double pos   = player.getCurrentPositionSeconds();
    const double total = player.getTotalLengthSeconds();

    if (total > 0.0)
        positionSlider.setValue (pos, juce::dontSendNotification);

    elapsedLabel  .setText (formatTime (pos),                         juce::dontSendNotification);
    remainingLabel.setText (formatTime (juce::jmax (0.0, total)),     juce::dontSendNotification);
}

void MiniPlayerComponent::onTransportButton()
{
    // The button is a one-way Stop while a track is playing. It's
    // disabled when no track is loaded — clicking it then is a no-op.
    if (player.isPlaying())
        player.stop();
}

void MiniPlayerComponent::refreshFromPlayer()
{
    const bool playing = player.isPlaying();
    transportButton.setShowingStop (playing);
    transportButton.setEnabled (playing);
    positionSlider.setEnabled  (playing);

    if (playing)
    {
        const double total = player.getTotalLengthSeconds();
        positionSlider.setRange (0.0, total > 0.0 ? total : 1.0, 0.0);
        const double pos   = player.getCurrentPositionSeconds();
        if (! userIsDragging)
            positionSlider.setValue (pos, juce::dontSendNotification);
        elapsedLabel  .setText (formatTime (pos),   juce::dontSendNotification);
        remainingLabel.setText (formatTime (total), juce::dontSendNotification);
    }
    else
    {
        positionSlider.setValue (0.0, juce::dontSendNotification);
        elapsedLabel  .setText ({}, juce::dontSendNotification);
        remainingLabel.setText ({}, juce::dontSendNotification);
    }

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
