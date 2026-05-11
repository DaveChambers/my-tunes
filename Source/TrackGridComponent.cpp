#include "TrackGridComponent.h"

#include "EmojiTagBoardComponent.h"
#include "TrackFile.h"
#include "TrackLibrary.h"
#include "TrackPreviewPlayer.h"

namespace
{
    constexpr int kCardW          = 200;
    constexpr int kCardH          = 240;
    constexpr int kGutter         = 12;
    constexpr int kOuterMargin    = 12;
    constexpr int kBottomPadding  = 16;  // breathing room below last row
    constexpr int kPlayDiam       = 56;

    int columnsFor (int width)
    {
        const int usable = juce::jmax (kCardW, width - 2 * kOuterMargin);
        const int cols   = juce::jmax (1, (usable + kGutter) / (kCardW + kGutter));
        return cols;
    }

    void paintTransportOverlay (juce::Graphics& g,
                                juce::Rectangle<int> rect,
                                bool isPlaying)
    {
        const auto fb = rect.toFloat();
        // Semi-transparent disc
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (fb);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawEllipse (fb.reduced (1.0f), 1.0f);

        const float cx = fb.getCentreX();
        const float cy = fb.getCentreY();
        const float r  = fb.getWidth() * 0.22f;

        g.setColour (juce::Colours::white);
        if (isPlaying)
        {
            const float side = r * 1.6f;
            g.fillRect (juce::Rectangle<float> (cx - side / 2,
                                                 cy - side / 2,
                                                 side, side));
        }
        else
        {
            juce::Path p;
            p.startNewSubPath (cx - r * 0.55f, cy - r);
            p.lineTo          (cx - r * 0.55f, cy + r);
            p.lineTo          (cx + r,         cy);
            p.closeSubPath();
            g.fillPath (p);
        }
    }
}

// =============================================================================
// CardsView — inner scrollable content. Painting + hit-testing live here so
// the viewport can clip them naturally and the bottom row is never hidden
// behind the mini-player / footer strip.
// =============================================================================
class TrackGridComponent::CardsView : public juce::Component
{
public:
    CardsView (TrackGridComponent& ownerIn) : owner (ownerIn) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff202229));

        const int n = owner.numVisible();
        if (n == 0)
        {
            g.setColour (juce::Colours::lightgrey);
            g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 13.0f, 0)));
            g.drawText  ("(no tracks)", getLocalBounds(), juce::Justification::centred);
            return;
        }

        const int cols       = columnsFor (getWidth());
        const int playingIdx = owner.player.getPlayingTrackIndex();

        for (int i = 0; i < n; ++i)
        {
            const int libIdx = owner.visibleAt (i);
            if (libIdx < 0) continue;
            const auto rect    = cardRect (i, cols);
            const bool playing = (libIdx == playingIdx);
            paintCard (g, libIdx, rect, i == selectedCardIndex, playing);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        const int  i = cardIndexAt (p);

        if (i < 0)
        {
            selectedCardIndex = -1;
            repaint();
            if (owner.onTrackSelected) owner.onTrackSelected (-1);
            return;
        }

        const int libIdx = owner.visibleAt (i);
        if (libIdx < 0)
            return;

        if (e.mods.isPopupMenu())
        {
            selectedCardIndex = i;
            repaint();
            showTagPopupMenu (libIdx, e.getScreenPosition());
            return;
        }

        const int  cols  = columnsFor (getWidth());
        const auto rect  = cardRect (i, cols);
        auto       inner = rect.reduced (10);
        const auto cover = inner.removeFromTop (140);
        const auto playRect = playHitRect (cover);

        if (playRect.contains (p))
        {
            if (owner.onPlayToggleRequested)
                owner.onPlayToggleRequested (libIdx);
            return;
        }

        selectedCardIndex = i;
        repaint();
        if (owner.onTrackSelected) owner.onTrackSelected (libIdx);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = cardIndexAt (e.getPosition());
        if (i < 0) return;
        const int libIdx = owner.visibleAt (i);
        if (libIdx < 0) return;

        selectedCardIndex = i;
        repaint();
        if (owner.onTrackSelected)        owner.onTrackSelected (libIdx);
        if (owner.onPlayToggleRequested)  owner.onPlayToggleRequested (libIdx);
    }

    // Total height needed to fit all cards at the current width, with
    // bottom padding so the final row has breathing room above the
    // viewport's bottom edge.
    int requiredHeightForWidth (int width) const
    {
        const int n = owner.numVisible();
        if (n == 0)
            return juce::jmax (120, kCardH + 2 * kOuterMargin);

        const int cols = columnsFor (width);
        const int rows = (n + cols - 1) / cols;
        return kOuterMargin + rows * kCardH + (rows - 1) * kGutter
             + kBottomPadding;
    }

    void clearSelection() { selectedCardIndex = -1; }

private:
    juce::Rectangle<int> cardRect (int cardIndex, int cols) const
    {
        const int row = cardIndex / cols;
        const int col = cardIndex % cols;
        const int x = kOuterMargin + col * (kCardW + kGutter);
        const int y = kOuterMargin + row * (kCardH + kGutter);
        return { x, y, kCardW, kCardH };
    }

    juce::Rectangle<int> playHitRect (juce::Rectangle<int> coverArea) const
    {
        return juce::Rectangle<int> (0, 0, kPlayDiam, kPlayDiam)
                  .withCentre (coverArea.getCentre());
    }

    int cardIndexAt (juce::Point<int> p) const
    {
        const int n    = owner.numVisible();
        const int cols = columnsFor (getWidth());
        for (int i = 0; i < n; ++i)
            if (cardRect (i, cols).contains (p))
                return i;
        return -1;
    }

    void paintCard (juce::Graphics& g, int libraryIndex,
                    juce::Rectangle<int> bounds, bool selected,
                    bool isPlayingThis) const
    {
        if (libraryIndex < 0 || libraryIndex >= owner.library.getNumTracks())
            return;

        const auto& tf = owner.library.getTrack (libraryIndex);

        g.setColour (selected ? juce::Colour (0xff3a4156) : juce::Colour (0xff262830));
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);

        if (selected)
        {
            g.setColour (juce::Colour (0xff7d8db8));
            g.drawRoundedRectangle (bounds.toFloat().reduced (1.0f), 8.0f, 1.5f);
        }

        auto inner = bounds.reduced (10);

        // ---- cover area + transport overlay ----------------------------
        auto cover = inner.removeFromTop (140);
        g.setColour (juce::Colour (0xff14151a));
        g.fillRoundedRectangle (cover.toFloat(), 4.0f);

        juce::Image art;
        if (tf.cachedArtworkPath.existsAsFile())
            art = juce::ImageCache::getFromFile (tf.cachedArtworkPath);

        if (art.isValid())
        {
            g.drawImageWithin (art,
                               cover.getX(), cover.getY(),
                               cover.getWidth(), cover.getHeight(),
                               juce::RectanglePlacement::centred,
                               false);
        }
        else
        {
            g.setColour (juce::Colours::lightgrey.withAlpha (0.5f));
            g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
            const juce::String placeholder =
                (tf.discogsStatus == TrackFile::DiscogsStatus::Searching)
                    ? "(searching...)"
                    : ((tf.discogsStatus == TrackFile::DiscogsStatus::NotFound
                      || tf.discogsStatus == TrackFile::DiscogsStatus::Error)
                           ? "(no cover)"
                           : "(no cover yet)");
            g.drawText (placeholder, cover, juce::Justification::centred);
        }

        paintTransportOverlay (g, playHitRect (cover), isPlayingThis);

        inner.removeFromTop (8);

        g.setColour (juce::Colours::white);
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, juce::Font::bold)));
        g.drawText  (tf.artist,
                     inner.removeFromTop (16),
                     juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));
        g.drawText  (tf.title.isNotEmpty() ? tf.title : tf.filename,
                     inner.removeFromTop (16),
                     juce::Justification::centredLeft, true);

        inner.removeFromTop (4);

        {
            auto row = inner.removeFromTop (14);

            juce::String keyBpm;
            if (tf.key.isNotEmpty()) keyBpm << tf.key;
            if (tf.bpm > 0)
            {
                if (keyBpm.isNotEmpty()) keyBpm << "  |  ";
                keyBpm << juce::String (tf.bpm) << " BPM";
            }
            g.setColour (juce::Colours::lightgrey);
            g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 10.5f, 0)));
            g.drawText  (keyBpm,
                         row.removeFromLeft (row.getWidth() / 2),
                         juce::Justification::centredLeft);

            // Right-side label tells Dave which API supplied the cover (or
            // the per-track Spotify state when nothing came back yet).
            juce::String sourceText;
            if (tf.artworkSource == TrackFile::ArtworkSource::Spotify)
                sourceText = "Spotify";
            else if (tf.artworkSource == TrackFile::ArtworkSource::Discogs)
                sourceText = "Discogs";
            else
                sourceText = TrackFile::spotifyStatusText (tf.spotifyStatus);

            g.setColour (juce::Colours::lightgrey.withAlpha (0.7f));
            g.drawText  (sourceText,
                         row,
                         juce::Justification::centredRight);
        }

        if (! tf.emojiTags.isEmpty())
        {
            g.setColour (juce::Colours::white);
            g.setFont   (juce::Font (juce::FontOptions ("Helvetica", 14.0f, 0)));
            g.drawText  (tf.emojiTags.joinIntoString (" "),
                         inner.removeFromTop (18),
                         juce::Justification::centredLeft);
        }
    }

    void showTagPopupMenu (int trackIndex, juce::Point<int> screenPos)
    {
        if (trackIndex < 0 || trackIndex >= owner.library.getNumTracks())
            return;

        const auto& tf = owner.library.getTrack (trackIndex);

        juce::PopupMenu menu;
        menu.addSectionHeader ("Tag with...");

        const auto& boards = EmojiTagBoardComponent::getBoards();
        for (int i = 0; i < (int) boards.size(); ++i)
        {
            const auto& b      = boards[(std::size_t) i];
            const bool  hasTag = tf.emojiTags.contains (b.emoji);
            menu.addItem (i + 1, b.emoji + "  " + b.label,
                          /*isActive=*/ true, /*isTicked=*/ hasTag);
        }

        auto options = juce::PopupMenu::Options()
                           .withTargetScreenArea (juce::Rectangle<int> (screenPos.x,
                                                                         screenPos.y,
                                                                         1, 1));

        auto& lib = owner.library;
        menu.showMenuAsync (options,
                            [trackIndex, &lib] (int chosen)
        {
            if (chosen <= 0) return;
            const auto& boardsCb = EmojiTagBoardComponent::getBoards();
            const int   boardIdx = chosen - 1;
            if (boardIdx < 0 || boardIdx >= (int) boardsCb.size()) return;
            lib.toggleEmojiTag (trackIndex, boardsCb[(std::size_t) boardIdx].emoji);
        });
    }

    TrackGridComponent& owner;
    int                 selectedCardIndex = -1;
};

// =============================================================================
// TrackGridComponent — viewport host
// =============================================================================

TrackGridComponent::TrackGridComponent (TrackLibrary& l, TrackPreviewPlayer& p)
    : library (l), player (p)
{
    cards = std::make_unique<CardsView> (*this);

    viewport.setViewedComponent (cards.get(), /*deleteComponentWhenViewportDeleted*/ false);
    viewport.setScrollBarsShown (/*vertical*/ true, /*horizontal*/ false);
    viewport.setColour (juce::ScrollBar::backgroundColourId,
                        juce::Colour (0xff202229));
    addAndMakeVisible (viewport);

    library.addChangeListener (this);
    player .addChangeListener (this);
}

TrackGridComponent::~TrackGridComponent()
{
    library.removeChangeListener (this);
    player .removeChangeListener (this);
}

void TrackGridComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void TrackGridComponent::resized()
{
    viewport.setBounds (getLocalBounds());

    // CardsView width = viewport width minus the vertical scrollbar
    // when it's actually shown, else the full inner width.
    const int contentW = viewport.getMaximumVisibleWidth();
    const int contentH = cards->requiredHeightForWidth (contentW);
    cards->setSize (contentW, contentH);
}

void TrackGridComponent::refresh()
{
    // Recompute content height (track count may have changed) and
    // repaint. Width is whatever the viewport is currently showing.
    const int contentW = viewport.getMaximumVisibleWidth();
    const int contentH = cards->requiredHeightForWidth (contentW);
    cards->setSize (contentW, contentH);
    cards->repaint();
}

void TrackGridComponent::setVisibleTrackIndices (std::vector<int> indices)
{
    visibleIndices = std::move (indices);
    cards->clearSelection();
    refresh();
}

int TrackGridComponent::numVisible() const
{
    return visibleIndices.empty() ? library.getNumTracks()
                                  : (int) visibleIndices.size();
}

int TrackGridComponent::visibleAt (int n) const
{
    if (n < 0) return -1;
    if (visibleIndices.empty())
        return n < library.getNumTracks() ? n : -1;
    if (n < (int) visibleIndices.size())
        return visibleIndices[(std::size_t) n];
    return -1;
}
