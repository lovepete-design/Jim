#include "EffectsProcessor.h"
#include "EffectsEditor.h"
juce::AudioProcessorValueTreeState::ParameterLayout EffectsProcessor::createParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& p : triadfx::parameters)
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{p.id, 1}, p.label,
            juce::NormalisableRange<float>(p.min, p.max, p.step, p.skew), p.initial,
            juce::AudioParameterFloatAttributes().withLabel(p.suffix)));
    for (size_t i = 0; i < triadfx::flagIds.size(); ++i)
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{triadfx::flagIds[i], 1},
            triadfx::flagLabels[i], triadfx::flagDefaults[i]));
    return result;
}
EffectsProcessor::EffectsProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, triadfx::stateName, createParameters()) {
    for (size_t i = 0; i < controls.size(); ++i) controls[i] = state.getRawParameterValue(triadfx::parameters[i].id);
    for (size_t i = 0; i < flags.size(); ++i) flags[i] = state.getRawParameterValue(triadfx::flagIds[i]);
    for (auto& meter : meters) meter.store(-180);
}
bool EffectsProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto input = layout.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) && input == layout.getMainOutputChannelSet();
}
void EffectsProcessor::prepareToPlay(double rate, int) {
    engine.prepare(rate);
#if TRIAD_MASTERING
    setLatencySamples(engine.latency());
#else
    setLatencySamples(0);
#endif
}
void EffectsProcessor::render(juce::AudioBuffer<float>& buffer, bool hostBypass) {
    juce::ScopedNoDenormals guard;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
#if TRIAD_MASTERING
    triadfx::MasterSettings s;
    s.input = controls[0]->load(); s.highpass = controls[1]->load(); s.low = controls[2]->load();
    s.midFrequency = controls[3]->load(); s.mid = controls[4]->load(); s.high = controls[5]->load();
    s.threshold = controls[6]->load(); s.ratio = controls[7]->load(); s.attack = controls[8]->load(); s.release = controls[9]->load();
    s.makeup = controls[10]->load(); s.saturation = controls[11]->load() * 0.01; s.width = controls[12]->load() * 0.01;
    s.monoBass = controls[13]->load(); s.loudness = controls[14]->load(); s.ceiling = controls[15]->load();
    s.eq = flags[0]->load() > 0.5f; s.compressor = flags[1]->load() > 0.5f; s.limiter = flags[2]->load() > 0.5f;
    s.bypass = hostBypass || flags[3]->load() > 0.5f;
#else
    triadfx::DeEssSettings s;
    s.frequency = controls[0]->load(); s.threshold = controls[1]->load(); s.ratio = controls[2]->load(); s.range = controls[3]->load();
    s.attack = controls[4]->load(); s.release = controls[5]->load(); s.mix = controls[6]->load() * 0.01; s.output = controls[7]->load();
    s.wide = flags[0]->load() > 0.5f; s.listen = flags[1]->load() > 0.5f; s.bypass = hostBypass || flags[2]->load() > 0.5f;
#endif
    engine.process(buffer.getArrayOfWritePointers(), std::min(2, buffer.getNumChannels()), buffer.getNumSamples(), s);
    const auto m = engine.meters;
    const float values[] {m.input, m.output, m.rms, m.reduction, m.limiting, m.detector};
    for (size_t i = 0; i < meters.size(); ++i) meters[i].store(values[i], std::memory_order_relaxed);
}
void EffectsProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { render(b, false); }
void EffectsProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { render(b, true); }
void EffectsProcessor::getStateInformation(juce::MemoryBlock& data) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, data);
}
void EffectsProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* EffectsProcessor::createEditor() { return new EffectsEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EffectsProcessor(); }
