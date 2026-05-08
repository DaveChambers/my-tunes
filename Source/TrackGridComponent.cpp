#include "TrackGridComponent.h"

#include "TrackLibrary.h"

TrackGridComponent::TrackGridComponent (TrackLibrary& l) : library (l) {}
TrackGridComponent::~TrackGridComponent() = default;

void TrackGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202229));
    g.setColour (juce::Colours::lightgrey);
    g.drawText ("Grid view (placeholder — tiles land in a later commit)",
                getLocalBounds(), juce::Justification::centred);
}

void TrackGridComponent::resized() {}

void TrackGridComponent::refresh()
{
    repaint();
}
