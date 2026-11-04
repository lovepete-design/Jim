#include "PhaserProcessor.h"
#include "PhaserEditor.h"
juce::AudioProcessorValueTreeState::ParameterLayout PhaserProcessor::parameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& p : phaseControls)
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{p.id, 1}, p.label,
            juce::NormalisableRange<float>(p.min, p.max, p.step, p.skew), p.initial,
            juce::AudioParameterFloatAttributes().withLabel(p.suffix)));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Character", juce::StringArray{"Smooth", "Resonant"}, 0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"stages", 1}, "Stages", juce::StringArray{"4 stages", "2 stages"}, 0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"division", 1}, "Sync period",
        juce::StringArray{"1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars"}, 2));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync", 1}, "Tempo sync", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    return result;
}
PhaserProcessor::PhaserProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "TriadPhaseState", parameters()) {
    for (size_t i = 0; i < values.size(); ++i) values[i] = state.getRawParameterValue(phaseControls[i].id);
    mode = state.getRawParameterValue("mode"); stageCount = state.getRawParameterValue("stages"); sync = state.getRawParameterValue("sync");
    division = state.getRawParameterValue("division"); bypass = state.getRawParameterValue("bypass");
}
bool PhaserProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto input = layout.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) && input == layout.getMainOutputChannelSet();
}
void PhaserProcessor::prepareToPlay(double rate, int) { engine.prepare(rate); setLatencySamples(0); }
void PhaserProcessor::render(juce::AudioBuffer<float>& audio, bool hostBypass) {
    juce::ScopedNoDenormals guard;
    for (int ch = getTotalNumInputChannels(); ch < audio.getNumChannels(); ++ch) audio.clear(ch, 0, audio.getNumSamples());
    if (audio.getNumChannels() == 0 || audio.getNumSamples() == 0) return;
    triadphase::Settings s;
    s.rate = values[0]->load(); s.depth = values[1]->load() * 0.01; s.feedback = values[2]->load() * 0.01;
    s.mix = values[3]->load() * 0.01; s.stereoDegrees = values[4]->load(); s.outputDb = values[5]->load();
    s.resonant = mode->load() > 0.5f; s.twoStages = stageCount->load() > 0.5f; s.sync = sync->load() > 0.5f;
    s.division = static_cast<int>(division->load()); s.bypass = hostBypass || bypass->load() > 0.5f;
    double bpm = 120;
    if (auto* host = getPlayHead()) if (auto position = host->getPosition()) if (auto tempo = position->getBpm())
        if (std::isfinite(*tempo)) bpm = *tempo;
    engine.process(audio.getArrayOfWritePointers(), std::min(2, audio.getNumChannels()), audio.getNumSamples(), s, bpm);
    sweep.store(static_cast<float>(engine.lfo), std::memory_order_relaxed);
    outputPeak.store(static_cast<float>(engine.outputPeak), std::memory_order_relaxed);
    leftFrequency.store(static_cast<float>(engine.frequency[0]), std::memory_order_relaxed);
}
void PhaserProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) { render(audio, false); }
void PhaserProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) { render(audio, true); }
void PhaserProcessor::getStateInformation(juce::MemoryBlock& data) { if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, data); }
void PhaserProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* PhaserProcessor::createEditor() { return new PhaserEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PhaserProcessor(); }
