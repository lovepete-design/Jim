#include "EffectsProcessor.h"
#include <iostream>
void check(bool value, const char* text) {
    if (!value) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    EffectsProcessor original;
    original.prepareToPlay(48000, 256);
    check(original.getLatencySamples() == (TRIAD_MASTERING ? 96 : 0), "Correct host latency");
    auto* threshold = original.state.getParameter("threshold");
    threshold->setValueNotifyingHost(threshold->convertTo0to1(-29));
    juce::MemoryBlock saved; original.getStateInformation(saved);
    EffectsProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize())); restored.prepareToPlay(48000, 256);
    check(std::abs(restored.state.getRawParameterValue("threshold")->load() + 29) < 0.01f, "Parameter state survives project recall");
    juce::MidiBuffer midi;
    for (int block = 0; block < 100; ++block) {
        juce::AudioBuffer<float> a(2, 256), b(2, 256);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) {
            const float x = static_cast<float>(0.8 * std::sin(2 * triadfx::pi * 7000 * (block * 256 + i) / 48000));
            a.setSample(ch, i, x); b.setSample(ch, i, x);
        }
        original.processBlock(a, midi); restored.processBlock(b, midi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i)
            check(std::abs(a.getSample(ch, i) - b.getSample(ch, i)) < 1e-7f, "Recalled processor produces identical output");
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(original.createEditor());
    check(editor && editor->getWidth() > 500, "Native editor constructs correctly");
    std::cout << "PASS: " << triadfx::productName << " host latency, parameter recall, rendered output, and native editor\n";
}
