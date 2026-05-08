#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

// The five emoji "buckets" that live in the left sidebar. A user
// drags a track onto one of these to add the matching emoji tag.
// First version: hard-coded boards, no drag-and-drop wired up yet.
class EmojiTagBoardComponent : public juce::Component
{
public:
    struct Board
    {
        juce::String emoji;
        juce::String label;
        int          trackCount = 0;
    };

    EmojiTagBoardComponent();
    ~EmojiTagBoardComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int          getNumBoards() const noexcept   { return (int) boards.size(); }
    const Board& getBoard (int index) const      { return boards[(std::size_t) index]; }

private:
    std::array<Board, 5> boards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmojiTagBoardComponent)
};
