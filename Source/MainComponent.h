#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// MainComponent — the window contents.
//
// First commit: just an opening title + status line so we can verify
// build and window. Folder picker / list / grid / boards land in
// subsequent commits.
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label attributionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
