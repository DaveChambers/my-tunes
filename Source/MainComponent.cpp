#include "MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("My Tunes", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions ("Helvetica", 28.0f, juce::Font::bold)));
    titleLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (statusLabel);
    statusLabel.setText ("Sample mode: 20 tracks   |   no folder selected",
                         juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (attributionLabel);
    attributionLabel.setText ("Data provided by Discogs.   "
                              "This application uses Discogs’ API but is not "
                              "affiliated with, sponsored or endorsed by Discogs.",
                              juce::dontSendNotification);
    attributionLabel.setJustificationType (juce::Justification::centred);
    attributionLabel.setFont (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
    attributionLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    setSize (1200, 800);
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a1f));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (24);
    titleLabel.setBounds       (bounds.removeFromTop (60));
    statusLabel.setBounds      (bounds.removeFromTop (28));
    attributionLabel.setBounds (bounds.removeFromBottom (32));
}
