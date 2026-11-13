#include "HexBrowser.h"
#include "Plugin.h"
#include <set>

#if TRIAD_KIND == 0
namespace
{
constexpr auto ink = 0xffedf0ed;
constexpr auto muted = 0xff8e9a9f;
constexpr auto panel = 0xff1a2026;

juce::String importSummary (const triad::HexLibrary::ImportReport& report)
{
    auto text = juce::String (report.added) + " sounds added";
    if (report.duplicates > 0) text += "  /  " + juce::String (report.duplicates) + " already in library";
    if (report.skipped > 0) text += "  /  " + juce::String (report.skipped) + " files skipped";
    if (report.error.isNotEmpty()) text += "  /  " + report.error;
    return text;
}
}

HexBrowser::HexBrowser (TriadProcessor& p, juce::File libraryRoot)
    : processor (p), library (std::make_shared<triad::HexLibrary> (std::move (libraryRoot)))
{
    setName ("Sound library");
    setComponentID ("hex-library");
    title.setText ("SOUND LIBRARY", juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, juce::Colour (muted));
    loaded.setColour (juce::Label::textColourId, juce::Colour (ink));
    loaded.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    status.setColour (juce::Label::textColourId, juce::Colour (muted));
    status.setFont (juce::Font (juce::FontOptions (12.0f)));
    search.setName ("Library search");
    search.setComponentID ("hex-search");
    search.setTextToShowWhenEmpty ("Search sounds or folders", juce::Colour (muted));
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff11181d));
    search.setColour (juce::TextEditor::textColourId, juce::Colour (ink));
    search.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff35414a));
    search.setTooltip ("Search the names and folder paths of your imported sounds.");
    search.onTextChange = [this] { filter(); };
    search.onEscapeKey = [this] { search.setText ({}, true); };
    folderFilter.setName ("Library folders");
    folderFilter.setComponentID ("hex-folders");
    folderFilter.setTooltip ("Browse a folder and all of its subfolders.");
    folderFilter.onChange = [this] { filter(); };
    folderFilter.addItem ("All folders", 1);
    folderFilter.setSelectedId (1, juce::dontSendNotification);
    importFolder.setName ("Import sound folder");
    importFolder.setTooltip ("Copy a folder and all nested .syx / .sysex sounds into your permanent Hex library.");
    importFolder.onClick = [this] { chooseFolder(); };
    importFiles.setTooltip ("Add one or more DX7 voices or banks. Every sound in a bank is imported.");
    importFiles.onClick = [this] { chooseFiles(); };
    refreshButton.setComponentID ("hex-refresh");
    refreshButton.setTooltip ("Find sounds imported from other Hex instances.");
    refreshButton.onClick = [this] { refresh(); };
    previous.setName ("Previous sound");
    previous.setComponentID ("hex-prev");
    next.setComponentID ("hex-next");
    next.setName ("Next sound");
    previous.setTooltip ("Load the previous sound in the current search and folder.");
    next.setTooltip ("Load the next sound in the current search and folder.");
    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    emptyState.setColour (juce::Label::textColourId, juce::Colour (muted));
    emptyState.setFont (juce::Font (juce::FontOptions (14.0f)));
    emptyState.setJustificationType (juce::Justification::centred);
    emptyState.setInterceptsMouseClicks (false, false);
    list.setComponentID ("hex-voices");
    list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff11181d));
    list.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff35414a));
    list.setOutlineThickness (1);
    list.setRowHeight (30);
    list.setMultipleSelectionEnabled (false);
    list.setClickingTogglesRowSelection (false);
    list.setTooltip ("Click a sound to play it. Use the up/down arrow keys or the previous/next buttons to browse.");
    for (auto* child : std::initializer_list<juce::Component*>
         { &title, &loaded, &status, &importFolder, &importFiles, &refreshButton,
           &previous, &next, &search, &folderFilter, &list, &emptyState })
        addAndMakeVisible (child);
    syncFromProcessor();
    {
        const std::lock_guard<std::mutex> lock (processor.libraryViewCache->mutex);
        auto& cache = *processor.libraryViewCache;
        const auto root = library->root().getFullPathName();
        if (cache.root != root) {
            cache.root = root; cache.ready = false; cache.entries.clear();
            cache.search.clear(); cache.folder.clear();
        }
        if (cache.ready) {
            entries = cache.entries;
            rebuildFolders();
            const int index = folders.indexOf (cache.folder);
            folderFilter.setSelectedId (index >= 0 ? index + 2 : 1, juce::dontSendNotification);
            search.setText (cache.search, false);
            filter();
            return;
        }
    }
    refresh();
}

HexBrowser::~HexBrowser()
{
    // Jobs own the backend, never this component or the processor. Completion
    // callbacks use a SafePointer and are harmless after the editor closes.
    {
        const std::lock_guard<std::mutex> lock (processor.libraryViewCache->mutex);
        processor.libraryViewCache->search = search.getText();
        processor.libraryViewCache->folder = folderFilter.getSelectedId() > 1 ? folderFilter.getText() : juce::String();
    }
    library->requestCancel();
    worker.removeAllJobs (true, -1);
    list.setModel (nullptr);
}

void HexBrowser::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (panel));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
}

void HexBrowser::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto heading = area.removeFromTop (28);
    title.setBounds (heading.removeFromLeft (132));
    next.setBounds (heading.removeFromRight (34));
    heading.removeFromRight (6);
    previous.setBounds (heading.removeFromRight (34));
    loaded.setBounds (heading.reduced (6, 0));
    area.removeFromTop (7);
    auto controls = area.removeFromTop (30);
    importFolder.setBounds (controls.removeFromLeft (132));
    controls.removeFromLeft (7);
    importFiles.setBounds (controls.removeFromLeft (100));
    controls.removeFromLeft (7);
    refreshButton.setBounds (controls.removeFromLeft (76));
    controls.removeFromLeft (12);
    search.setBounds (controls.removeFromLeft (250));
    controls.removeFromLeft (9);
    folderFilter.setBounds (controls);
    area.removeFromTop (9);
    status.setBounds (area.removeFromBottom (23));
    list.setBounds (area);
    emptyState.setBounds (area.reduced (20, 8));
}

int HexBrowser::getNumRows() { return (int) filtered.size(); }

void HexBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || (size_t) row >= filtered.size()) return;
    const auto& entry = entries[filtered[(size_t) row]];
    if (selected)
    {
        g.setColour (processor.accent().withAlpha (0.22f));
        g.fillAll();
        g.setColour (processor.accent());
        g.fillRect (0, 0, 3, height);
    }
    else if ((row & 1) != 0)
    {
        g.setColour (juce::Colour (0xffffffff).withAlpha (0.025f));
        g.fillAll();
    }
    g.setColour (juce::Colour (ink));
    g.setFont (juce::Font (juce::FontOptions (14.0f, selected ? juce::Font::bold : juce::Font::plain)));
    const int nameWidth = juce::jmin (330, width * 2 / 5);
    g.drawText (entry.name, 12, 0, nameWidth - 20, height, juce::Justification::centredLeft);
    g.setColour (juce::Colour (muted));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (entry.folder, nameWidth, 0, width - nameWidth - 16, height, juce::Justification::centredLeft);
}

void HexBrowser::selectedRowsChanged (int row)
{
    if (! changingSelection) loadRow (row);
}

void HexBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! changingSelection) loadRow (row);
}

void HexBrowser::returnKeyPressed (int row) { loadRow (row); }

void HexBrowser::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Import your DX7 sounds folder (including subfolders)", lastImport);
    const juce::Component::SafePointer<HexBrowser> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe] (const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto folder = dialog.getResult();
            if (! folder.isDirectory()) return;
            safe->lastImport = folder;
            safe->runTask ([folder] (triad::HexLibrary& lib) { return importSummary (lib.importFolder (folder)); });
        });
}

void HexBrowser::chooseFiles()
{
    chooser = std::make_unique<juce::FileChooser> ("Add DX7 voices or banks", lastImport, "*.syx;*.sysex");
    const juce::Component::SafePointer<HexBrowser> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe] (const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto files = dialog.getResults();
            if (files.isEmpty()) return;
            safe->lastImport = files[0].getParentDirectory();
            safe->runTask ([files] (triad::HexLibrary& lib) { return importSummary (lib.importFiles (files)); });
        });
}

void HexBrowser::refresh()
{
    runTask ([] (triad::HexLibrary& lib)
    {
        juce::String error;
        lib.reload (&error);
        return error;
    });
}

void HexBrowser::runTask (std::function<juce::String(triad::HexLibrary&)> task)
{
    if (busy) return;
    busy = true;
    emptyState.setVisible (false);
    status.setText ("Reading sounds... You can keep playing while the library updates.", juce::dontSendNotification);
    updateButtons();
    const juce::Component::SafePointer<HexBrowser> safe (this);
    const auto backend = library;
    const auto cache = processor.libraryViewCache;
    worker.addJob ([safe, backend, cache, task = std::move (task)]
    {
        const auto message = task (*backend);
        auto snapshot = backend->entries();
        if (! message.containsIgnoreCase ("cancel")) {
            const std::lock_guard<std::mutex> lock (cache->mutex);
            cache->entries = snapshot;
            cache->ready = true;
        }
        juce::MessageManager::callAsync ([safe, snapshot = std::move (snapshot), message]() mutable
        {
            if (safe == nullptr) return;
            safe->entries = std::move (snapshot);
            safe->busy = false;
            safe->rebuildFolders();
            safe->filter();
            if (message.isNotEmpty()) safe->status.setText (message, juce::dontSendNotification);
            safe->repaint();
        });
    });
}

void HexBrowser::rebuildFolders()
{
    const auto selected = folderFilter.getSelectedId() > 1 ? folderFilter.getText() : juce::String();
    folders.clear();
    std::set<juce::String> uniqueFolders;
    for (const auto& entry : entries)
    {
        auto path = entry.folder;
        while (path.isNotEmpty())
        {
            if (! uniqueFolders.insert (path).second) break;
            const auto slash = path.lastIndexOfChar ('/');
            if (slash < 0) break;
            path = path.substring (0, slash);
        }
    }
    for (const auto& path : uniqueFolders) folders.add (path);
    folders.sort (true);
    folderFilter.clear (juce::dontSendNotification);
    folderFilter.addItem ("All folders", 1);
    folderFilter.addItemList (folders, 2);
    const int index = folders.indexOf (selected);
    folderFilter.setSelectedId (index >= 0 ? index + 2 : 1, juce::dontSendNotification);
}

void HexBrowser::filter()
{
    filtered.clear();
    auto terms = juce::StringArray::fromTokens (search.getText().trim(), " ", "\"");
    terms.removeEmptyStrings();
    const auto folder = folderFilter.getSelectedId() > 1 ? folderFilter.getText() : juce::String();
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (folder.isNotEmpty() && entry.folder != folder && ! entry.folder.startsWith (folder + "/")) continue;
        const auto searchable = entry.name + " " + entry.folder;
        bool matches = true;
        for (const auto& term : terms)
            if (! searchable.containsIgnoreCase (term)) { matches = false; break; }
        if (matches) filtered.push_back (i);
    }
    changingSelection = true;
    list.updateContent();
    changingSelection = false;
    selectLoadedRow();
    auto text = juce::String ((int) filtered.size()) + " sounds";
    if (filtered.size() != entries.size()) text += " shown / " + juce::String ((int) entries.size()) + " in library";
    text += entries.empty() ? "  /  Import Folder to get started." : "  /  Click a sound, then play. Up/down arrows browse the list.";
    if (! busy) status.setText (text, juce::dontSendNotification);
    emptyState.setText (entries.empty()
        ? "Import your top-level sounds folder once. Its subfolders and bank voices will appear here."
        : "No sounds match this search and folder.", juce::dontSendNotification);
    emptyState.setVisible (! busy && filtered.empty());
    updateButtons();
    repaint();
}

void HexBrowser::loadRow (int row)
{
    if (row < 0 || (size_t) row >= filtered.size()) return;
    const auto& entry = entries[filtered[(size_t) row]];
    if (processor.loadedVoiceId() != entry.id)
        processor.loadLibraryVoice (entry.patch, entry.name, entry.id);
    syncFromProcessor();
}

void HexBrowser::step (int direction)
{
    if (filtered.empty()) return;
    int current = -1;
    const auto id = processor.loadedVoiceId();
    for (size_t i = 0; i < filtered.size(); ++i)
        if (entries[filtered[i]].id == id) { current = (int) i; break; }
    const int count = (int) filtered.size();
    const int row = current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
    loadRow (row);
    list.scrollToEnsureRowIsOnscreen (row);
}

void HexBrowser::selectLoadedRow()
{
    const auto id = processor.loadedVoiceId();
    juce::SparseSet<int> selection;
    for (size_t i = 0; i < filtered.size(); ++i)
        if (entries[filtered[i]].id == id) { selection.addRange ({ (int) i, (int) i + 1 }); break; }
    changingSelection = true;
    list.setSelectedRows (selection, juce::dontSendNotification);
    if (! selection.isEmpty()) list.scrollToEnsureRowIsOnscreen (selection[0]);
    changingSelection = false;
}

void HexBrowser::syncFromProcessor()
{
    const auto name = processor.loadedVoiceName();
    const auto id = processor.loadedVoiceId();
    if (lastName == name && lastId == id) return;
    lastName = name;
    lastId = id;
    loaded.setText (name.isEmpty() ? "Current sound" : name, juce::dontSendNotification);
    loaded.setTooltip ("Current sound: " + name + ". The complete voice is saved in your Ableton project.");
    selectLoadedRow();
    list.repaint();
}

void HexBrowser::updateButtons()
{
    importFolder.setEnabled (! busy);
    importFiles.setEnabled (! busy);
    refreshButton.setEnabled (! busy);
    previous.setEnabled (! filtered.empty());
    next.setEnabled (! filtered.empty());
}
#endif
