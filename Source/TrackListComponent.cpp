#include "TrackListComponent.h"

#include "TrackLibrary.h"

TrackListComponent::TrackListComponent (TrackLibrary& l) : library (l) {}
TrackListComponent::~TrackListComponent() = default;

void TrackListComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202229));
    g.setColour (juce::Colours::lightgrey);
    g.drawText ("List view (placeholder — table lands in next commit)",
                getLocalBounds(), juce::Justification::centred);
}

void TrackListComponent::resized() {}

void TrackListComponent::refresh()
{
    repaint();
}
