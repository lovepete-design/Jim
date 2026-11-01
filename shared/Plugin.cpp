#include "Plugin.h"
#include "Editor.h"
#include <algorithm>
#include <cmath>

namespace {
using Float = juce::AudioParameterFloat;
juce::ParameterID id(const char* name) { return {name, 1}; }
void addFloat(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
              const char* key, const char* label, float lo, float hi, float initial,
              const char* unit = "", float skew = 1.0f)
{
    juce::NormalisableRange<float> range{lo, hi};
    range.skew = skew;
    layout.add(std::make_unique<Float>(id(key), label, range, initial,
                  juce::AudioParameterFloatAttributes{}.withLabel(unit)));
}
}

juce::AudioProcessorValueTreeState::ParameterLayout TriadProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
#if TRIAD_KIND == 0
    addFloat(p,"output","Output",-36,6,-12,"dB");
    addFloat(p,"tune","Fine tune",-100,100,0,"ct");
    p.add(std::make_unique<juce::AudioParameterInt>(id("algorithm"),"Algorithm",1,32,1));
    p.add(std::make_unique<juce::AudioParameterInt>(id("feedback"),"Feedback",0,7,0));
#elif TRIAD_KIND == 1
    addFloat(p,"input","Input",-24,36,0,"dB");
    addFloat(p,"output","Output",-24,24,0,"dB");
    addFloat(p,"attack","Attack",.05f,30,5,"ms",.35f);
    addFloat(p,"release","Release",40,3500,100,"ms",.35f);
    p.add(std::make_unique<juce::AudioParameterChoice>(id("ratio"),"Ratio",juce::StringArray{"1:1","2:1","3:1","4:1","6:1","10:1","20:1","Limit"},3));
    addFloat(p,"mix","Mix",0,100,100,"%");
    p.add(std::make_unique<juce::AudioParameterChoice>(id("distortion"),"Harmonics",juce::StringArray{"Clean","Even","Odd"},0));
    p.add(std::make_unique<juce::AudioParameterBool>(id("highpass"),"Detector high pass",false));
    p.add(std::make_unique<juce::AudioParameterBool>(id("emphasis"),"Detector emphasis",false));
#else
    addFloat(p,"diffusion","Diffusion",0,1,.35f);
    addFloat(p,"tension","Tension",0,1,.25f);
    addFloat(p,"memory","Memory",0,1,.5f);
    addFloat(p,"brightness","Brightness",0,1,.5f);
    addFloat(p,"attack","Attack",1,2000,20,"ms",.35f);
    addFloat(p,"release","Release",20,6000,700,"ms",.35f);
    addFloat(p,"output","Output",-36,6,-9,"dB");
#endif
    return p;
}

TriadProcessor::TriadProcessor()
    : AudioProcessor(BusesProperties()
#if TRIAD_KIND == 1
                     .withInput("Input",juce::AudioChannelSet::stereo(),true)
#endif
                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"TRIAD_STATE",createLayout())
{
#if TRIAD_KIND == 0
    const char* names[]{"output","tune","algorithm","feedback"};
    savedPatch = engine.patch();
#elif TRIAD_KIND == 1
    const char* names[]{"input","output","attack","release","ratio","mix","distortion","highpass","emphasis"};
#else
    const char* names[]{"diffusion","tension","memory","brightness","attack","release","output"};
#endif
    size_t index=0;
    for (auto name : names) atoms[index++]=parameters.getRawParameterValue(name);
    keyboardState.addListener(this);
}
TriadProcessor::~TriadProcessor() { keyboardState.removeListener(this); }

juce::String TriadProcessor::title() const
{
#if TRIAD_KIND == 0
    return "Hex Library";
#elif TRIAD_KIND == 1
    return "Stress";
#else
    return "Lattice";
#endif
}
juce::String TriadProcessor::subtitle() const
{
#if TRIAD_KIND == 0
    return "SIX OPERATOR FM / DX7 ENGINE";
#elif TRIAD_KIND == 1
    return "DYNAMICS / HARMONIC COLOUR";
#else
    return "COUPLED SPECTRAL TRANSPORT";
#endif
}
juce::Colour TriadProcessor::accent() const
{
#if TRIAD_KIND == 0
    return juce::Colour(0xff9ee8b6);
#elif TRIAD_KIND == 1
    return juce::Colour(0xfff0ac6b);
#else
    return juce::Colour(0xffaaa4ff);
#endif
}
bool TriadProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    if (layout.getMainOutputChannelSet()!=juce::AudioChannelSet::stereo()) return false;
#if TRIAD_KIND == 1
    return layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo();
#else
    return layout.getMainInputChannelSet().isDisabled();
#endif
}
void TriadProcessor::prepareToPlay(double sr,int)
{
    engine.prepare(sr);
    engine.reset();
    meterValue.store(0);
#if TRIAD_KIND == 0
    setLatencySamples(engine.latencySamples());
#endif
    updateEngineParameters();
}
void TriadProcessor::releaseResources() { resetRequested.store(true); }
void TriadProcessor::updateEngineParameters()
{
#if TRIAD_KIND == 0
    triad::HexParameters p;
    p.outputDb=raw(0); p.tuneCents=raw(1); p.algorithm=juce::roundToInt(raw(2))-1; p.feedback=juce::roundToInt(raw(3));
#elif TRIAD_KIND == 1
    constexpr float ratios[]{1,2,3,4,6,10,20,100};
    triad::DynamicsParameters p;
    p.inputDb=raw(0); p.outputDb=raw(1); p.attackMs=raw(2); p.releaseMs=raw(3);
    p.ratio=ratios[juce::jlimit(0,7,juce::roundToInt(raw(4)))]; p.mix=raw(5)*.01f;
    p.distortion=juce::roundToInt(raw(6)); p.detectorHighpass=raw(7)>.5f; p.emphasis=raw(8)>.5f;
#else
    triad::LatticeParameters p;
    p.diffusion=raw(0); p.tension=raw(1); p.memory=raw(2); p.brightness=raw(3);
    p.attackMs=raw(4); p.releaseMs=raw(5); p.outputDb=raw(6);
#endif
    engine.setParameters(p);
}
void TriadProcessor::handleMidi(const juce::MidiMessage& message)
{
#if TRIAD_KIND != 1
    if (message.isNoteOn()) engine.noteOn(message.getNoteNumber(),message.getVelocity());
    else if (message.isNoteOff()) engine.noteOff(message.getNoteNumber());
    else if (message.isSustainPedalOn()) engine.sustain(true);
    else if (message.isSustainPedalOff()) engine.sustain(false);
    else if (message.isPitchWheel()) engine.pitchBend((message.getPitchWheelValue()-8192)*(2.f/8192.f));
    else if (message.isAllNotesOff() || message.isAllSoundOff()) engine.reset();
#else
    juce::ignoreUnused(message);
#endif
}
void TriadProcessor::render(juce::AudioBuffer<float>& buffer,int start,int length)
{
    if (length<=0) return;
#if TRIAD_KIND == 1
    engine.process(buffer.getWritePointer(0,start),buffer.getWritePointer(1,start),length);
#else
    engine.render(buffer.getWritePointer(0,start),buffer.getWritePointer(1,start),length);
#endif
}
void TriadProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels()<2) { buffer.clear(); midi.clear(); return; }
    if (resetRequested.exchange(false)) engine.reset();
#if TRIAD_KIND == 0
    if ((patchMiddle.load(std::memory_order_acquire)&4)!=0)
    {
        patchFront=patchMiddle.exchange(patchFront,std::memory_order_acq_rel)&3;
        engine.setPatch(patches[static_cast<size_t>(patchFront)]);
    }
#endif
    updateEngineParameters();
#if TRIAD_KIND != 1
    buffer.clear();
    int first,count,second,count2;
    keyboardQueue.prepareToRead(128,first,count,second,count2);
    const auto playKeyboard=[this](const KeyboardEvent& e)
    {
        if(e.velocity>0) engine.noteOn(e.note,e.velocity); else engine.noteOff(e.note);
    };
    for(int i=0;i<count;++i) playKeyboard(keyboardEvents[static_cast<size_t>(first+i)]);
    for(int i=0;i<count2;++i) playKeyboard(keyboardEvents[static_cast<size_t>(second+i)]);
    keyboardQueue.finishedRead(count+count2);
    if(keyboardOverflow.exchange(false)) engine.reset();
    int position=0;
    for (auto metadata : midi)
    {
        const int event=juce::jlimit(position,buffer.getNumSamples(),metadata.samplePosition);
        render(buffer,position,event-position);
        if (metadata.numBytes<=3) handleMidi(metadata.getMessage());
        position=event;
    }
    render(buffer,position,buffer.getNumSamples()-position);
#else
    render(buffer,0,buffer.getNumSamples());
#endif
    for (int channel=2;channel<buffer.getNumChannels();++channel) buffer.clear(channel,0,buffer.getNumSamples());
    midi.clear();
#if TRIAD_KIND == 1
    meterValue.store(engine.gainReductionDb(),std::memory_order_relaxed);
#else
    const float peak=std::max(buffer.getMagnitude(0,0,buffer.getNumSamples()),buffer.getMagnitude(1,0,buffer.getNumSamples()));
    meterValue.store(std::max(peak,meterValue.load()*.92f),std::memory_order_relaxed);
#endif
}
void TriadProcessor::queueKeyboard(int note,int velocity)
{
    int first,count,second,count2;
    keyboardQueue.prepareToWrite(1,first,count,second,count2);
    if(count+count2!=1) { keyboardOverflow.store(true); return; }
    keyboardEvents[static_cast<size_t>(count?first:second)]={note,velocity};
    keyboardQueue.finishedWrite(1);
}
void TriadProcessor::handleNoteOn(juce::MidiKeyboardState*,int,int note,float velocity)
{
    queueKeyboard(note,juce::jlimit(1,127,juce::roundToInt(velocity*127)));
}
void TriadProcessor::handleNoteOff(juce::MidiKeyboardState*,int,int note,float)
{
    queueKeyboard(note,0);
}
void TriadProcessor::setValue(const char* key,float value)
{
    if (auto* parameter=parameters.getParameter(key))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}
juce::StringArray TriadProcessor::presetNames() const
{
#if TRIAD_KIND == 0
    return {"Glass tine","Pure six","Metal struck"};
#elif TRIAD_KIND == 1
    return {"Studio control","Drum crush","Vocal ride","Parallel weight"};
#else
    return {"Violet currents","Glass roots","Slow weather","Wire bloom"};
#endif
}
void TriadProcessor::selectPreset(int index)
{
    if (!juce::isPositiveAndBelow(index,presetNames().size())) return;
#if TRIAD_KIND == 0
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
    // Start each preset from the same authored electric-piano patch.
    triad::HexEngine defaults;
    auto patch=defaults.patch();
    if (index==1)
    {
        patch[134]=31; patch[135]=0;
        for(int op=0;op<6;++op) { patch[static_cast<size_t>(op*21+16)]=65; patch[static_cast<size_t>(op*21+18)]=1; patch[static_cast<size_t>(op*21+19)]=0; }
    }
    else if (index==2)
    {
        patch[134]=4; patch[135]=6;
        for(int op=0;op<6;++op) patch[static_cast<size_t>(op*21+18)]=static_cast<unsigned char>(1+op*2);
    }
    if (!queuePatch(patch)) return;
    savedVoiceName=presetNames()[index];
    savedVoiceId.clear();
    setValue("algorithm",patch[134]+1.f); setValue("feedback",patch[135]); setValue("tune",0); setValue("output",-12);
#elif TRIAD_KIND == 1
    const float values[][9]={{0,0,5,100,3,100,0,0,0},{18,-8,.3f,70,6,100,2,1,0},{8,1,8,240,2,100,1,1,1},{20,-6,10,350,4,30,1,1,0}};
    const char* names[]{"input","output","attack","release","ratio","mix","distortion","highpass","emphasis"};
    for(size_t i=0;i<9;++i) setValue(names[i],values[index][i]);
#else
    const float values[][7]={{.35f,.25f,.5f,.5f,20,700,-9},{.1f,.15f,.2f,.8f,2,1300,-9},{.8f,.4f,.95f,.3f,900,4500,-9},{.55f,.85f,.65f,.75f,35,1800,-12}};
    const char* names[]{"diffusion","tension","memory","brightness","attack","release","output"};
    for(size_t i=0;i<7;++i) setValue(names[i],values[index][i]);
#endif
    currentProgram.store(index);
    resetRequested.store(true);
}
#if TRIAD_KIND == 0
bool TriadProcessor::queuePatch(const Patch& patch)
{
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
    patches[static_cast<size_t>(patchBack)]=patch;
    savedPatch=patch;
    patchBack=patchMiddle.exchange(patchBack|4,std::memory_order_acq_rel)&3;
    return true;
}
void TriadProcessor::loadLibraryVoice(const Patch& patch,const juce::String& name,const juce::String& voiceId)
{
    // Library entries have passed the strict DX7 parser. The handoff is the same
    // bounded, lock-free triple buffer used for SysEx and project restoration.
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
    queuePatch(patch);
    savedVoiceName=name.substring(0,128);
    savedVoiceId=voiceId.substring(0,1024);
    setValue("algorithm",patch[134]+1.f);
    setValue("feedback",patch[135]);
}
juce::String TriadProcessor::loadedVoiceName() const
{
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
    return savedVoiceName;
}
juce::String TriadProcessor::loadedVoiceId() const
{
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
    return savedVoiceId;
}
#endif
juce::Result TriadProcessor::loadSysexFile(const juce::File& file)
{
#if TRIAD_KIND == 0
    if (file.getSize()>65536) return juce::Result::fail("Choose one DX7 single-voice or 32-voice SysEx file.");
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data)) return juce::Result::fail("The file could not be read.");
    Patch patch{};
    if (!triad::HexEngine::parseSysex(static_cast<const unsigned char*>(data.getData()),data.getSize(),patch))
        return juce::Result::fail("Invalid DX7 SysEx format, parameter range, or checksum.");
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
    queuePatch(patch);
    savedVoiceName=file.getFileNameWithoutExtension();
    savedVoiceId.clear();
    setValue("algorithm",patch[134]+1.f); setValue("feedback",patch[135]);
    return juce::Result::ok();
#else
    juce::ignoreUnused(file);
    return juce::Result::fail("SysEx import belongs to Hex Library.");
#endif
}
void TriadProcessor::getStateInformation(juce::MemoryBlock& destination)
{
#if TRIAD_KIND == 0
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
#endif
    auto state=parameters.copyState();
    state.setProperty("kind",TRIAD_KIND,nullptr);
    state.setProperty("program",currentProgram.load(),nullptr);
#if TRIAD_KIND == 0
    {
        state.setProperty("patch",juce::Base64::toBase64(savedPatch.data(),savedPatch.size()),nullptr);
        state.setProperty("voiceName",savedVoiceName,nullptr);
        state.setProperty("voiceId",savedVoiceId,nullptr);
    }
#endif
    if (auto xml=state.createXml()) copyXmlToBinary(*xml,destination);
}
void TriadProcessor::setStateInformation(const void* data,int size)
{
#if TRIAD_KIND == 0
    const std::lock_guard<std::recursive_mutex> guard(patchProducerMutex);
#endif
    auto xml=getXmlFromBinary(data,size);
    if (!xml || !xml->hasTagName("TRIAD_STATE")) return;
    auto state=juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(state.getProperty("kind",-1))!=TRIAD_KIND) return;
#if TRIAD_KIND == 0
    juce::MemoryOutputStream decoded;
    if (!juce::Base64::convertFromBase64(decoded,state.getProperty("patch").toString()) || decoded.getDataSize()!=155) return;
    Patch patch{};
    std::copy_n(static_cast<const unsigned char*>(decoded.getData()),155,patch.begin());
    // Parameter-range validation is shared with the strict SysEx parser.
    std::array<unsigned char,163> sysex{};
    sysex[0]=0xf0; sysex[1]=0x43; sysex[2]=0; sysex[3]=0; sysex[4]=1; sysex[5]=0x1b;
    std::copy(patch.begin(),patch.end(),sysex.begin()+6);
    unsigned sum=0; for(auto byte:patch) sum+=byte;
    sysex[161]=static_cast<unsigned char>((128-(sum&127))&127); sysex[162]=0xf7;
    Patch checked{};
    if (!triad::HexEngine::parseSysex(sysex.data(),sysex.size(),checked) || !queuePatch(checked)) return;
    savedVoiceName=state.getProperty("voiceName",juce::String::fromUTF8(reinterpret_cast<const char*>(checked.data()+145),10).trim()).toString().substring(0,128);
    savedVoiceId=state.getProperty("voiceId").toString().substring(0,1024);
#endif
    parameters.replaceState(state);
    currentProgram.store(juce::jlimit(0,presetNames().size()-1,static_cast<int>(state.getProperty("program",0))));
    resetRequested.store(true);
}
juce::AudioProcessorEditor* TriadProcessor::createEditor() { return new TriadEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TriadProcessor(); }
