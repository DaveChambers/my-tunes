#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ---- toolbar -----------------------------------------------------------
    addAndMakeVisible (chooseFolderButton);
    chooseFolderButton.onClick = [this] { onChooseFolderClicked(); };

    addAndMakeVisible (scanButton);
    scanButton.onClick = [this] { runScan(); };
    scanButton.setEnabled (false);

    addAndMakeVisible (folderLabel);
    folderLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    folderLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 12.0f, 0)));

    // ---- main area ---------------------------------------------------------
    addAndMakeVisible (emojiBoard);
    addAndMakeVisible (listView);
    addAndMakeVisible (detailView);

    // ---- attribution footer ------------------------------------------------
    addAndMakeVisible (attributionLabel);
    attributionLabel.setText ("Data provided by Discogs.   "
                              "This application uses Discogs’ API but is not "
                              "affiliated with, sponsored or endorsed by Discogs.",
                              juce::dontSendNotification);
    attributionLabel.setJustificationType (juce::Justification::centred);
    attributionLabel.setFont   (juce::Font (juce::FontOptions ("Helvetica", 11.0f, 0)));
    attributionLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    updateFolderLabel();
    updateStatusLabel();

    setSize (1200, 800);
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a1f));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Footer
    attributionLabel.setBounds (bounds.removeFromBottom (28).reduced (12, 4));

    // Toolbar
    auto toolbar = bounds.removeFromTop (44).reduced (12, 6);
    chooseFolderButton.setBounds (toolbar.removeFromLeft (140));
    toolbar.removeFromLeft (8);
    scanButton.setBounds         (toolbar.removeFromLeft (90));
    toolbar.removeFromLeft (12);

    statusLabel.setBounds        (toolbar.removeFromRight (220));
    folderLabel.setBounds        (toolbar);

    // Main area: left sidebar | list | detail
    bounds.removeFromTop (4);
    bounds.removeFromBottom (4);

    auto sidebar = bounds.removeFromLeft (240);
    auto detail  = bounds.removeFromRight (320);
    emojiBoard.setBounds (sidebar.reduced (8));
    detailView.setBounds (detail.reduced (8));
    listView.setBounds   (bounds.reduced (4));
}

void MainComponent::onChooseFolderClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose a folder of tunes",
        getDefaultStartFolder(),
        juce::String());

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (folder == juce::File{} || ! folder.isDirectory())
            return;

        library.scanFolder (juce::File{}, kSampleSize); // clear, then scan
        library.scanFolder (folder,        kSampleSize);
        listView.refresh();
        detailView.setSelectedTrackIndex (-1);
        updateFolderLabel();
        updateStatusLabel();
        scanButton.setEnabled (true);
    });
}

void MainComponent::runScan()
{
    if (! library.getFolder().isDirectory())
        return;

    library.scanFolder (library.getFolder(), kSampleSize);
    listView.refresh();
    updateStatusLabel();
}

void MainComponent::updateFolderLabel()
{
    const auto f = library.getFolder();
    folderLabel.setText (f == juce::File{} ? juce::String ("(no folder selected)")
                                            : f.getFullPathName(),
                         juce::dontSendNotification);
}

void MainComponent::updateStatusLabel()
{
    const int n = library.getNumTracks();
    juce::String text;
    text << "Sample mode: " << kSampleSize << " tracks   |   "
         << n << " loaded";
    statusLabel.setText (text, juce::dontSendNotification);
}

juce::File MainComponent::getDefaultStartFolder()
{
    const juce::File daveExample
        ("/Volumes/Data/Tunes/Dave's Tunes 2020 [KEYED WITH SWIFT APP]");
    if (daveExample.isDirectory())
        return daveExample;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}
