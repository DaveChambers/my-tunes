#include "EmojiTagBoardComponent.h"

EmojiTagBoardComponent::EmojiTagBoardComponent()
{
    boards = {
        Board { juce::String::fromUTF8 ("\xF0\x9F\x99\x82"), "Rave / fun",          0 },
        Board { juce::String::fromUTF8 ("\xF0\x9F\x8C\x85"), "Sunset / chill",      0 },
        Board { juce::String::fromUTF8 ("\xF0\x9F\x94\xA5"), "Peak time",           0 },
        Board { juce::String::fromUTF8 ("\xF0\x9F\x95\xBA"), "Old school house",    0 },
        Board { juce::String::fromUTF8 ("\xE2\xAD\x90"),     "Must play",           0 }
    };
}

EmojiTagBoardComponent::~EmojiTagBoardComponent() = default;

void EmojiTagBoardComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141519));

    auto bounds = getLocalBounds().reduced (10);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 14.0f, juce::Font::bold)));
    g.drawText  ("Boards", bounds.removeFromTop (24), juce::Justification::centredLeft);

    bounds.removeFromTop (8);

    for (const auto& b : boards)
    {
        auto row = bounds.removeFromTop (54);
        g.setColour (juce::Colour (0xff262830));
        g.fillRoundedRectangle (row.toFloat(), 8.0f);

        auto inner = row.reduced (12, 6);
        auto emojiArea = inner.removeFromLeft (40);
        g.setColour (juce::Colours::white);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 22.0f, 0)));
        g.drawText  (b.emoji, emojiArea, juce::Justification::centredLeft);

        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 14.0f, juce::Font::bold)));
        g.drawText  (b.label, inner.removeFromTop (inner.getHeight() / 2),
                     juce::Justification::centredLeft);
        g.setColour (juce::Colours::lightgrey);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
        g.drawText  (juce::String (b.trackCount) + " tracks", inner,
                     juce::Justification::centredLeft);

        bounds.removeFromTop (6);
    }
}

void EmojiTagBoardComponent::resized() {}
