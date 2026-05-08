#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "EmojiTagBoardComponent.h"
#include "TrackDetailComponent.h"
#include "TrackLibrary.h"
#include "TrackListComponent.h"

// Top-level window contents.
//
// Layout (this commit):
//   ┌──────────────── toolbar ─────────────────────┐
//   │ [Choose Folder] folder/path  [Scan]  status   │
//   ├─────────┬───────────────────────────┬────────┤
//   │ emoji   │      list view            │ detail │
//   │ boards  │                           │        │
//   ├─────────┴───────────────────────────┴────────┤
//   │           Discogs attribution                 │
//   └───────────────────────────────────────────────┘
//
// View toggle (List / Grid), search box, "Lookup Covers" button, and
// drag-and-drop tagging arrive in later commits.
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // --- model -------------------------------------------------------------
    TrackLibrary library;

    // --- toolbar -----------------------------------------------------------
    juce::TextButton chooseFolderButton { "Choose Folder" };
    juce::TextButton scanButton         { "Scan" };
    juce::Label      folderLabel;
    juce::Label      statusLabel;
    juce::Label      attributionLabel;

    // --- main area ---------------------------------------------------------
    EmojiTagBoardComponent emojiBoard;
    TrackListComponent     listView   { library };
    TrackDetailComponent   detailView { library };

    // --- file-chooser must outlive its callback ---------------------------
    std::unique_ptr<juce::FileChooser> fileChooser;

    void onChooseFolderClicked();
    void runScan();
    void updateFolderLabel();
    void updateStatusLabel();

    // First place we suggest in the file chooser. Falls back to ~/Music if
    // Dave's example path isn't present.
    static juce::File getDefaultStartFolder();

    // First-version sample limit. Surfaced in the status line.
    static constexpr int kSampleSize = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
