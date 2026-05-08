#include "EmojiTagBoardComponent.h"

const std::array<EmojiTagBoardComponent::BoardInfo, 5>&
EmojiTagBoardComponent::getBoards()
{
    // UTF-8 byte sequences for the emoji literals — kept escaped so the
    // source file is portable in any text editor that doesn't render
    // emoji glyphs in source view.
    static const std::array<BoardInfo, 5> boards {
        BoardInfo { juce::String::fromUTF8 ("\xF0\x9F\x99\x82"), "Rave / fun" },        // 🙂
        BoardInfo { juce::String::fromUTF8 ("\xF0\x9F\x8C\x85"), "Sunset / chill" },    // 🌅
        BoardInfo { juce::String::fromUTF8 ("\xF0\x9F\x94\xA5"), "Peak time" },         // 🔥
        BoardInfo { juce::String::fromUTF8 ("\xF0\x9F\x95\xBA"), "Old school house" }, // 🕺
        BoardInfo { juce::String::fromUTF8 ("\xE2\xAD\x90"),     "Must play" }          // ⭐
    };
    return boards;
}

EmojiTagBoardComponent::EmojiTagBoardComponent()
{
    boardCounts.fill (0);
    boardRects.fill ({});
}

EmojiTagBoardComponent::~EmojiTagBoardComponent() = default;

void EmojiTagBoardComponent::resized()
{
    recomputeRects();
}

void EmojiTagBoardComponent::recomputeRects()
{
    auto bounds = getLocalBounds().reduced (10);
    bounds.removeFromTop (24);                  // "Boards" header
    bounds.removeFromTop (8);                   // gap

    for (int i = 0; i < (int) boardRects.size(); ++i)
    {
        boardRects[(std::size_t) i] = bounds.removeFromTop (54);
        bounds.removeFromTop (6);               // gap between boards
    }
}

void EmojiTagBoardComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141519));

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 14.0f, juce::Font::bold)));
    g.drawText  ("Boards",
                 getLocalBounds().reduced (10).withHeight (24),
                 juce::Justification::centredLeft);

    const auto& boards = getBoards();
    for (int i = 0; i < (int) boards.size(); ++i)
    {
        const auto& b   = boards[(std::size_t) i];
        const auto  row = boardRects[(std::size_t) i];
        if (row.isEmpty())
            continue;

        const bool active = (i == activeBoardIndex);
        g.setColour (active ? juce::Colour (0xff3a4156)
                            : juce::Colour (0xff262830));
        g.fillRoundedRectangle (row.toFloat(), 8.0f);

        if (active)
        {
            g.setColour (juce::Colour (0xff7d8db8));
            g.drawRoundedRectangle (row.toFloat().reduced (1.0f), 8.0f, 1.5f);
        }

        auto inner     = row.reduced (12, 6);
        auto emojiArea = inner.removeFromLeft (40);
        g.setColour (juce::Colours::white);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 22.0f, 0)));
        g.drawText  (b.emoji, emojiArea, juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions ("Helvetica", 14.0f, juce::Font::bold)));
        g.drawText (b.label,
                    inner.removeFromTop (inner.getHeight() / 2),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colours::lightgrey);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
        g.drawText  (juce::String (boardCounts[(std::size_t) i]) + " tracks",
                     inner, juce::Justification::centredLeft);
    }
}

void EmojiTagBoardComponent::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) boardRects.size(); ++i)
    {
        if (boardRects[(std::size_t) i].contains (e.getPosition()))
        {
            if (onBoardClicked)
                onBoardClicked (i);
            return;
        }
    }
}

void EmojiTagBoardComponent::setBoardCount (int boardIndex, int count)
{
    if (boardIndex < 0 || boardIndex >= (int) boardCounts.size()) return;
    if (boardCounts[(std::size_t) boardIndex] == count)           return;
    boardCounts[(std::size_t) boardIndex] = count;
    repaint();
}

void EmojiTagBoardComponent::setActiveBoardIndex (int index)
{
    if (index < -1 || index >= (int) getBoards().size()) index = -1;
    if (activeBoardIndex == index) return;
    activeBoardIndex = index;
    repaint();
}
