#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "HexLibrary.h"
#include <functional>
#include <memory>

class TriadProcessor;

// The managed library is read on one worker; selecting a sound uses its in-memory
// patch, so browsing never opens a file or touches the disk on the audio thread.
class HexBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit HexBrowser (TriadProcessor&, juce::File libraryRoot = triad::HexLibrary::defaultRoot());
    ~HexBrowser() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void syncFromProcessor();

private:
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void selectedRowsChanged (int) override;
    void listBoxItemClicked (int, const juce::MouseEvent&) override;
    void returnKeyPressed (int) override;
    void chooseFolder();
    void chooseFiles();
    void refresh();
    void runTask (std::function<juce::String(triad::HexLibrary&)>);
    void rebuildFolders();
    void filter();
    void loadRow (int);
    void step (int);
    void selectLoadedRow();
    void updateButtons();

    TriadProcessor& processor;
    std::shared_ptr<triad::HexLibrary> library;
    std::vector<triad::HexLibrary::Entry> entries;
    std::vector<size_t> filtered;
    juce::StringArray folders;
    juce::String lastName, lastId;
    juce::Label title, loaded, status, emptyState;
    juce::TextButton importFolder { "Import Folder..." }, importFiles { "Add Files..." },
                     refreshButton { "Refresh" }, previous { "<" }, next { ">" };
    juce::TextEditor search;
    juce::ComboBox folderFilter;
    juce::ListBox list { "Library voices", this };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastImport;
    bool busy = false, changingSelection = false;
    juce::ThreadPool worker { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HexBrowser)
};
