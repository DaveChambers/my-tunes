#include "TrackDetailComponent.h"

#include "TrackLibrary.h"

TrackDetailComponent::TrackDetailComponent (TrackLibrary& l) : library (l) {}
TrackDetailComponent::~TrackDetailComponent() = default;

void TrackDetailComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1c22));
    g.setColour (juce::Colours::lightgrey);
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 13.0f, 0)));
    g.drawText  (selectedIndex < 0 ? "Track details (no selection)"
                                   : "Track details (selected #"
                                         + juce::String (selectedIndex)
                                         + ")",
                getLocalBounds().reduced (12), juce::Justification::topLeft);
}

void TrackDetailComponent::resized() {}

void TrackDetailComponent::setSelectedTrackIndex (int index)
{
    selectedIndex = index;
    repaint();
}
