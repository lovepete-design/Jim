#include "SynthProcessor.h"
#include "SynthEditor.h"
juce::AudioProcessorValueTreeState::ParameterLayout OracleProcessor::parameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& p : oracleControls)
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{p.id, 1}, p.label,
            juce::NormalisableRange<float>(p.min, p.max, p.step, p.skew), p.initial,
            juce::AudioParameterFloatAttributes().withLabel(p.suffix)));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"wavea", 1}, "Oscillator A wave", juce::StringArray{"Saw", "Pulse", "Triangle"}, 0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"waveb", 1}, "Oscillator B wave", juce::StringArray{"Saw", "Pulse", "Triangle"}, 0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync", 1}, "A syncs B", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"keytrack", 1}, "Filter key tracking", true));
    return result;
}
OracleProcessor::OracleProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "TriadOracleState", parameters()) {
    for (size_t i = 0; i < values.size(); ++i) values[i] = state.getRawParameterValue(oracleControls[i].id);
    waveA = state.getRawParameterValue("wavea"); waveB = state.getRawParameterValue("waveb");
    sync = state.getRawParameterValue("sync"); keytrack = state.getRawParameterValue("keytrack");
}
bool OracleProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    return layout.getMainInputChannelSet().isDisabled() && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void OracleProcessor::prepareToPlay(double rate, int) { engine.prepare(rate); updateSettings(); setLatencySamples(0); }
void OracleProcessor::updateSettings() {
    // Normalised skewed host parameters can differ by a few float ULPs after
    // XML recall. Use their declared step consistently before driving phase,
    // sync, or envelope timing, where tiny differences can move an edge.
    auto value = [this](size_t index) {
        const auto& spec = oracleControls[index];
        const double step = spec.step, minimum = spec.min;
        const double raw = values[index]->load();
        return std::clamp(minimum + std::round((raw - minimum) / step) * step, minimum, static_cast<double>(spec.max));
    };
    oracle::Settings s;
    s.mix = value(0) * 0.01; s.coarse = value(1); s.detune = value(2);
    s.pulse = value(3) * 0.01; s.noise = value(4) * 0.01; s.cutoff = value(5);
    s.resonance = value(6) * 0.01; s.filterAmount = value(7);
    s.ampAttack = value(8); s.ampDecay = value(9); s.ampSustain = value(10) * 0.01; s.ampRelease = value(11);
    s.filterAttack = value(12); s.filterDecay = value(13); s.filterSustain = value(14) * 0.01; s.filterRelease = value(15);
    s.lfoRate = value(16); s.vibrato = value(17) * 0.01; s.filterLfo = value(18);
    s.polyB = value(19) * 0.01; s.polyEnv = value(20) * 0.01; s.chorus = value(21) * 0.01;
    s.spread = value(22) * 0.01; s.drift = value(23) * 0.01; s.volume = value(24);
    s.waveA = static_cast<int>(waveA->load()); s.waveB = static_cast<int>(waveB->load()); s.sync = sync->load() > 0.5f; s.keytrack = keytrack->load() > 0.5f;
    engine.setSettings(s);
}
void OracleProcessor::event(const juce::MidiMessage& m) {
    if (m.isNoteOn()) engine.noteOn(m.getChannel(), m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff(m.getChannel(), m.getNoteNumber());
    else if (m.isPitchWheel()) engine.pitchBend(m.getChannel(), m.getPitchWheelValue());
    else if (m.isController()) engine.control(m.getChannel(), m.getControllerNumber(), m.getControllerValue());
}
void OracleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals guard; buffer.clear();
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0) return;
    keyboard.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);
    updateSettings();
    if (panicRequested.exchange(false)) { engine.panic(); midi.clear(); }
    int position = 0; float peak = 0;
    for (const auto metadata : midi) {
        const int next = juce::jlimit(position, buffer.getNumSamples(), metadata.samplePosition);
        if (next > position) {
            engine.render(buffer.getWritePointer(0, position), buffer.getWritePointer(1, position), next - position);
            peak = std::max(peak, static_cast<float>(engine.peak));
        }
        event(metadata.getMessage()); position = next;
    }
    if (position < buffer.getNumSamples()) {
        engine.render(buffer.getWritePointer(0, position), buffer.getWritePointer(1, position), buffer.getNumSamples() - position);
        peak = std::max(peak, static_cast<float>(engine.peak));
    }
    voiceCount.store(engine.activeVoices(), std::memory_order_relaxed); outputPeak.store(peak, std::memory_order_relaxed);
}
void OracleProcessor::getStateInformation(juce::MemoryBlock& data) { if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, data); }
void OracleProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}
void OracleProcessor::applyPatch(int index) {
    if (index < 0 || index >= static_cast<int>(patchNames.size())) return;
    auto set = [this](const char* id, float value) {
        if (auto* p = state.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture(); }
    };
    for (const auto& p : oracleControls) set(p.id, p.initial);
    set("wavea", 0); set("waveb", 0); set("sync", 0); set("keytrack", 1);
    if (index == 1) { set("cutoff", 850); set("ampattack", 1200); set("amprelease", 2200); set("ampsustain", 85); set("filterattack", 1600); set("filteramount", 1.8f); set("chorus", 25); set("spread", 80); }
    if (index == 2) { set("cutoff", 1400); set("filteramount", 3.5f); set("filterdecay", 450); set("resonance", 35); set("detune", 11); }
    if (index == 3) { set("sync", 1); set("coarse", 12); set("mix", 65); set("polyenv", 35); set("cutoff", 2400); set("resonance", 32); set("chorus", 0); }
    if (index == 4) { set("wavea", 2); set("waveb", 2); set("coarse", 19); set("polyb", 18); set("ampdecay", 1500); set("ampsustain", 0); set("amprelease", 1200); set("cutoff", 5500); set("filteramount", 0); set("chorus", 18); }
    if (index == 5) { set("coarse", -12); set("mix", 35); set("cutoff", 400); set("filteramount", 2.8f); set("filterdecay", 240); set("filtersustain", 0); set("ampattack", 2); set("amprelease", 100); set("chorus", 0); set("spread", 0); }
    if (index == 6) { set("wavea", 2); set("waveb", 1); set("mix", 30); set("cutoff", 4400); set("filteramount", 1); set("ampdecay", 1100); set("ampsustain", 15); set("amprelease", 900); set("chorus", 20); }
    if (index == 7) { set("ampattack", 2200); set("amprelease", 3500); set("filterattack", 3000); set("cutoff", 600); set("filteramount", 3); set("lforate", 0.15f); set("filterlfo", 0.7f); set("chorus", 30); set("spread", 100); }
    if (index == 8) { set("wavea", 1); set("waveb", 1); set("pulse", 35); set("detune", 14); set("ampattack", 400); set("amprelease", 1400); set("cutoff", 2200); set("filteramount", 1); set("vibrato", 7); set("chorus", 28); }
    if (index == 9) { set("coarse", 7); set("cutoff", 1700); set("filteramount", 2.5f); set("ampattack", 70); set("chorus", 15); }
}
juce::AudioProcessorEditor* OracleProcessor::createEditor() { return new OracleEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OracleProcessor(); }
