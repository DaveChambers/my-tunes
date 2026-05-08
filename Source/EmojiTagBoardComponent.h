#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

// The five emoji "buckets" in the left sidebar. Click a board to filter
// the list/grid view to tracks carrying that emoji; click the active
// board again to clear the filter.
//
// Boards are static config — same emoji/label set is referenced from
// the right-click popup menu in TrackListComponent.
class EmojiTagBoardComponent : public juce::Component
{
public:
    struct BoardInfo
    {
        juce::String emoji;
        juce::String label;
    };

    EmojiTagBoardComponent();
    ~EmojiTagBoardComponent() override;

    void paint (juce::Graphics&)  override;
    void resized()                override;
    void mouseDown (const juce::MouseEvent& e) override;

    // Static config so other components (right-click popup, filter
    // logic) can iterate over the same set of boards.
    static const std::array<BoardInfo, 5>& getBoards();

    // Live state ----------------------------------------------------------

    // Push the current count next to a board's label.
    void setBoardCount (int boardIndex, int count);

    // Index of the currently-active filter board, or -1 if none.
    int  getActiveBoardIndex() const noexcept { return activeBoardIndex; }
    void setActiveBoardIndex (int index);     // -1 to clear

    // Notification: called whenever the user clicks a board. The handler
    // decides whether to engage / clear the filter and calls back into
    // setActiveBoardIndex().
    std::function<void (int boardIndex)> onBoardClicked;

private:
    std::array<int, 5>                  boardCounts {};
    std::array<juce::Rectangle<int>, 5> boardRects {};
    int                                  activeBoardIndex = -1;

    void recomputeRects();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmojiTagBoardComponent)
};
