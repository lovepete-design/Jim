#include "../shared/Plugin.h"
#include "../shared/HexBrowser.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

#if JUCE_MAC
namespace juce { void initialiseNSApplication(); }
#endif

namespace {
void require(bool condition,const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void dispatch(int milliseconds=20)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
void browserCheck(TriadProcessor& processor)
{
    const auto temporary=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("triad-browser-tests","",false);
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup{temporary};
    const auto source=temporary.getChildFile("My Sounds/Keys/Nested");
    require(source.createDirectory().wasOk(),"Could not create test sound folder");
    triad::HexEngine defaults;
    for (int index=0;index<2;++index)
    {
        auto patch=defaults.patch();
        patch[134]=static_cast<unsigned char>(index==0?4:31);
        const juce::String name=index==0?"FIRST":"SECOND";
        std::fill(patch.begin()+145,patch.end(),' ');
        std::copy_n(name.toRawUTF8(),name.length(),patch.begin()+145);
        std::array<unsigned char,163> syx{{0xf0,0x43,0,0,1,0x1b}};
        std::copy(patch.begin(),patch.end(),syx.begin()+6);
        unsigned sum=0; for (auto byte:patch) sum+=byte;
        syx[161]=static_cast<unsigned char>((0u-sum)&127u); syx[162]=0xf7;
        require(source.getChildFile(name+".sysex").replaceWithData(syx.data(),syx.size()),"Could not write test voice");
    }
    const auto root=temporary.getChildFile("Managed library");
    triad::HexLibrary library(root);
    require(library.importFolder(temporary.getChildFile("My Sounds")).added==2,"Could not import browser fixture");
    require(temporary.getChildFile("My Sounds").deleteRecursively(),"Could not remove original sounds");
    HexBrowser browser(processor,root);
    browser.setSize(1060,250);
    auto* list=dynamic_cast<juce::ListBox*>(browser.findChildWithID("hex-voices"));
    auto* search=dynamic_cast<juce::TextEditor*>(browser.findChildWithID("hex-search"));
    auto* folders=dynamic_cast<juce::ComboBox*>(browser.findChildWithID("hex-folders"));
    auto* next=dynamic_cast<juce::TextButton*>(browser.findChildWithID("hex-next"));
    auto* previous=dynamic_cast<juce::TextButton*>(browser.findChildWithID("hex-prev"));
    require(list && search && folders && next && previous,"Browser controls missing");
    for (int attempt=0;attempt<100 && list->getListBoxModel()->getNumRows()!=2;++attempt) dispatch();
    require(list->getListBoxModel()->getNumRows()==2,"Persisted browser failed to load asynchronously");
    require(folders->getNumItems()==4,"Nested browser folder hierarchy missing");
    list->selectRow(0);
    require(processor.loadedVoiceName()=="FIRST","Clicking a browser row did not select its patch");
    search->setText("SECOND",true); dispatch();
    require(list->getListBoxModel()->getNumRows()==1,"Browser search failed");
    require(processor.loadedVoiceName()=="FIRST","Filtering unexpectedly changed the current sound");
    next->triggerClick(); dispatch();
    require(processor.loadedVoiceName()=="SECOND","Next did not load the filtered sound");
    search->onEscapeKey(); dispatch();
    require(list->getListBoxModel()->getNumRows()==2,"Escape did not clear the search filter");
    previous->triggerClick(); dispatch();
    require(processor.loadedVoiceName()=="FIRST","Previous did not move through sounds");
    list->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    require(processor.loadedVoiceName()=="SECOND","Arrow-key sound browsing failed");
    // Folder filtering and a library refresh must preserve the selected voice.
    folders->setSelectedId(4,juce::sendNotificationSync); dispatch();
    require(list->getListBoxModel()->getNumRows()==2,"Nested folder filtering lost sounds");
    // Hide the managed library on disk. Reopening must use the processor cache
    // synchronously, rather than starting a disk reload and an empty list.
    const auto hidden = temporary.getChildFile ("Hidden library");
    require(root.moveFileTo(hidden), "Could not hide library for reopen test");
    {
        HexBrowser reopenedBrowser(processor,root);
        auto* reopenedList=dynamic_cast<juce::ListBox*>(reopenedBrowser.findChildWithID("hex-voices"));
        require(reopenedList && reopenedList->getListBoxModel()->getNumRows()==2,
                "Reopening browser rescanned disk instead of using cached sounds");
        dispatch(100);
        require(reopenedList->getListBoxModel()->getNumRows()==2,
                "Background reload replaced cached sounds on reopening");
    }
    require(hidden.moveFileTo(root), "Could not restore test library");
    std::cout<<"PASS: persisted browser, nested folders, search, previous/next, and arrow-key sound selection.\n";
}
std::vector<float> render(TriadProcessor& processor)
{
    processor.prepareToPlay(48000,256);
    juce::AudioBuffer<float> buffer(2,256);
    juce::MidiBuffer midi;
    std::vector<float> output;
    for (int block=0;block<32;++block)
    {
        buffer.clear(); midi.clear();
        if (block==2) midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),0);
        if (block==20) midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
        processor.processBlock(buffer,midi);
        for (int sample=0;sample<buffer.getNumSamples();++sample)
            output.push_back(buffer.getSample(0,sample));
    }
    return output;
}
}

int main()
{
    try
    {
       #if JUCE_MAC
        juce::initialiseNSApplication();
       #endif
        juce::ScopedJuceInitialiser_GUI initialise;
        TriadProcessor original;
        browserCheck(original);
        triad::HexEngine defaults;
        auto patch=defaults.patch();
        patch[134]=31; patch[135]=3;
        original.loadLibraryVoice(patch,"My nested / Brass sound","collections/brass/unique");
        juce::MemoryBlock state;
        original.getStateInformation(state);
        auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
        require(xml!=nullptr,"Missing saved XML");
        require(xml->getStringAttribute("patch")==juce::Base64::toBase64(patch.data(),patch.size()),"Library patch was not embedded in project");
        for (int i=0;i<40;++i) original.selectPreset(i%3);
        original.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        require(original.loadedVoiceName()=="My nested / Brass sound","Stopped preset changes lost imported name");
        require(original.loadedVoiceId()=="collections/brass/unique","Library selection was not restored");

        TriadProcessor reopened;
        reopened.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        require(reopened.loadedVoiceName()==original.loadedVoiceName(),"New instance lost imported name");
        const auto a=render(original),b=render(reopened);
        double energy=0;
        require(a.size()==b.size(),"Render length mismatch");
        for (size_t i=0;i<a.size();++i)
        {
            require(std::isfinite(a[i]) && std::isfinite(b[i]),"Non-finite library audio");
            require(std::abs(a[i]-b[i])<2.e-5f,"Imported sound changed on project restore");
            energy+=a[i]*a[i];
        }
        require(energy>1.e-6,"Library sound was silent");
        // Earlier versions saved the same patch bytes, without browser metadata.
        xml->removeAttribute("voiceName"); xml->removeAttribute("voiceId");
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
        reopened.setStateInformation(legacy.getData(),static_cast<int>(legacy.getSize()));
        require(reopened.loadedVoiceId().isEmpty(),"Legacy project retained stale library ID");
        require(reopened.loadedVoiceName().isNotEmpty(),"Legacy project has no sound name");
        reopened.selectPreset(1);
        require(reopened.loadedVoiceName()=="Pure six" && reopened.loadedVoiceId().isEmpty(),"Factory preset did not clear library selection");
        std::cout<<"PASS: imported patch/name/selection and audio survive new-instance project restore; legacy states supported.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr<<"Hex state validation failed: "<<error.what()<<'\n';
        return 1;
    }
}
