#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout TriadProcessor::makeParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"root", 1}, "Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale", 1}, "Scale",
        juce::StringArray{"Chromatic", "Major", "Natural minor", "Major pentatonic"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"retune", 1}, "Retune",
        juce::NormalisableRange<float>(0, 200, 0.1f, 0.5f), 30, juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"strength", 1}, "Correction",
        juce::NormalisableRange<float>(0, 100, 0.1f), 100, juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"humanize", 1}, "Tolerance",
        juce::NormalisableRange<float>(0, 50, 0.1f), 8, juce::AudioParameterFloatAttributes().withLabel("ct")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix", 1}, "Mix",
        juce::NormalisableRange<float>(0, 100, 0.1f), 100, juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output", 1}, "Output",
        juce::NormalisableRange<float>(-24, 12, 0.1f), 0, juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Mode",
        juce::StringArray{"Note editor", "Live tune"}, 0));
    return layout;
}
TriadProcessor::TriadProcessor()
    : AudioProcessor(BusesProperties().withInput("Vocal", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "TriadTuneState", makeParameters()) {
    const char* ids[] {"root", "scale", "retune", "strength", "humanize", "mix", "output", "bypass", "mode"};
    for (size_t i = 0; i < values.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    startTimerHz(20);
}
bool TriadProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && in == layouts.getMainOutputChannelSet();
}
void TriadProcessor::prepareToPlay(double rate, int) {
    engine.prepare(rate); clip.prepare(rate);
    liveLatency.store(engine.getLatency());
    setLatencySamples(values[8]->load() < 0.5f ? 0 : engine.getLatency());
    detected.store(-1); target.store(-1); confidence.store(0); level.store(0);
}
void TriadProcessor::render(juce::AudioBuffer<float>& buffer, bool bypassed) {
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
    const bool manual = values[8]->load() < 0.5f;
    const bool bypass = bypassed || values[7]->load() > 0.5f;
    if (manual) {
        if (auto* host = getPlayHead()) {
            if (auto position = host->getPosition()) {
                if (auto seconds = position->getTimeInSeconds()) {
                    playheadSeconds.store(*seconds, std::memory_order_relaxed);
                    clip.capture(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), *seconds, position->getIsPlaying());
                    if (!bypass && position->getIsPlaying())
                        clip.playback(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), *seconds);
                }
            }
        }
        return;
    }
    triad::Settings s;
    s.root = static_cast<int>(values[0]->load());
    s.scale = static_cast<triad::Scale>(static_cast<int>(values[1]->load()));
    s.retuneMs = values[2]->load(); s.strength = values[3]->load() * 0.01f;
    s.humanizeCents = values[4]->load(); s.mix = values[5]->load() * 0.01f; s.outputDb = values[6]->load();
    if (bypass) { s.mix = 0; s.outputDb = 0; }
    level.store(buffer.getRMSLevel(0, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    engine.process(buffer.getArrayOfWritePointers(), std::min(2, buffer.getNumChannels()), buffer.getNumSamples(), s);
    detected.store(static_cast<float>(engine.inputMidi), std::memory_order_relaxed);
    target.store(static_cast<float>(engine.targetMidi), std::memory_order_relaxed);
    confidence.store(static_cast<float>(engine.confidence), std::memory_order_relaxed);
}
void TriadProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { render(b, false); }
void TriadProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { render(b, true); }
void TriadProcessor::timerCallback() {
    clip.service();
    const int latency = values[8]->load() < 0.5f ? 0 : liveLatency.load();
    if (latency != getLatencySamples()) setLatencySamples(latency);
}
void TriadProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state = parameters.copyState();
    triad::ManualClip::Snapshot snapshot;
    { const juce::ScopedLock lock(getCallbackLock()); snapshot = clip.snapshot(); }
    if (!snapshot.audio[0].empty()) {
        juce::MemoryOutputStream audio;
        audio.writeInt(1); audio.writeDouble(snapshot.rate); audio.writeDouble(snapshot.origin);
        audio.writeInt(static_cast<int>(snapshot.audio[0].size()));
        for (const auto& channel : snapshot.audio) for (float value : channel) audio.writeFloat(value);
        audio.writeInt(static_cast<int>(snapshot.notes.size()));
        for (const auto& n : snapshot.notes) {
            audio.writeInt(n.start); audio.writeInt(n.end); audio.writeFloat(n.original); audio.writeInt(n.transpose); audio.writeBool(n.tuned);
        }
        state.setProperty("capture", juce::var(audio.getMemoryBlock()), nullptr);
    }
    juce::MemoryOutputStream output(data, false); state.writeToStream(output);
}
void TriadProcessor::setStateInformation(const void* data, int size) {
    auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(size));
    if (!state.isValid() || state.getType() != parameters.state.getType()) return;
    triad::ManualClip::Snapshot saved;
    if (auto* bytes = state.getProperty("capture").getBinaryData()) {
        juce::MemoryInputStream input(*bytes, false);
        if (input.readInt() != 1) return;
        saved.rate = input.readDouble(); saved.origin = input.readDouble();
        const int samples = input.readInt();
        if (!std::isfinite(saved.rate) || saved.rate < 8000 || saved.rate > 384000 || !std::isfinite(saved.origin)
            || samples < 0 || samples > saved.rate * 120 || input.getNumBytesRemaining() < static_cast<juce::int64>(samples) * 8 + 4) return;
        for (auto& channel : saved.audio) {
            channel.resize(static_cast<size_t>(samples));
            for (auto& value : channel) { value = input.readFloat(); if (!std::isfinite(value)) value = 0; }
        }
        const int notes = input.readInt();
        if (notes < 0 || notes > triad::ManualClip::maxNotes || input.getNumBytesRemaining() < notes * 17) return;
        for (int i = 0; i < notes; ++i) {
            const int start = input.readInt(), end = input.readInt();
            const float original = input.readFloat(); const int shift = input.readInt();
            const bool tuned = input.readBool();
            if (start < 0 || end < start || end > samples || !std::isfinite(original) || original < 0 || original > 127) return;
            saved.notes.push_back({start, end, original, shift, tuned});
        }
    }
    state.removeProperty("capture", nullptr);
    const juce::ScopedLock lock(getCallbackLock());
    parameters.replaceState(state); clip.restore(saved);
}
juce::AudioProcessorEditor* TriadProcessor::createEditor() { return new TriadEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TriadProcessor(); }
