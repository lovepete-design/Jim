#include "SynthProcessor.h"
#include <iostream>
void check(bool ok, const char* message) { if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    OracleProcessor original;
    original.applyPatch(3);
    juce::MemoryBlock data; original.getStateInformation(data);
    OracleProcessor restored; restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    check(restored.state.getRawParameterValue("sync")->load() > 0.5f, "Sync mode survives project recall");
    check(std::abs(restored.state.getRawParameterValue("coarse")->load() - 12) < 0.01f, "Oscillator tuning survives recall");
    for (const auto& spec : oracleControls)
        check(std::abs(original.state.getRawParameterValue(spec.id)->load() - restored.state.getRawParameterValue(spec.id)->load()) < std::max(0.001f, spec.step * 0.01f), "Patch controls survive recall at their declared precision");
    original.prepareToPlay(48000, 256); restored.prepareToPlay(48000, 256);
    check(original.acceptsMidi() && original.getLatencySamples() == 0, "Synth accepts MIDI without buffered latency");
    juce::AudioBuffer<float> a(2, 256), b(2, 256);
    for (int block = 0; block < 200; ++block) {
        juce::MidiBuffer m1, m2;
        if (block == 0) { m1.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 64); m2 = m1; }
        original.processBlock(a, m1); restored.processBlock(b, m2);
        if (block == 0) {
            check(a.getMagnitude(0, 0, 64) == 0, "Sample-offset MIDI produces no sound before note-on");
            check(a.getMagnitude(0, 64, 192) > 0, "MIDI note-on produces sound in the same block");
        }
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) {
            if (a.getSample(ch, i) != b.getSample(ch, i)) {
                std::cerr << "Recall mismatch at block " << block << ", sample " << i << ": " << a.getSample(ch, i) << " / " << b.getSample(ch, i) << '\n';
                for (const auto& spec : oracleControls) {
                    const float x = original.state.getRawParameterValue(spec.id)->load(), y = restored.state.getRawParameterValue(spec.id)->load();
                    if (x != y) std::cerr << spec.id << ": " << x << " / " << y << '\n';
                }
            }
            check(a.getSample(ch, i) == b.getSample(ch, i), "Recalled patch renders identically");
        }
    }
    original.panicRequested.store(true); juce::MidiBuffer empty; original.processBlock(a, empty);
    check(a.getMagnitude(0, 256) == 0, "Panic clears sounding notes and chorus tails");
    original.keyboard.noteOn(1, 64, 0.8f);
    original.processBlock(a, empty); check(a.getMagnitude(0, 256) > 0, "On-screen keyboard produces audio");
    for (int patch = 0; patch < static_cast<int>(patchNames.size()); ++patch) {
        OracleProcessor p; p.applyPatch(patch); p.prepareToPlay(48000, 256);
        bool audible = false;
        for (int block = 0; block < 220; ++block) {
            juce::MidiBuffer midi; if (block == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
            p.processBlock(a, midi); audible |= a.getMagnitude(0, 256) > 0.001f;
        }
        check(audible, "Every factory patch produces sound");
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(original.createEditor());
    check(editor && editor->getWidth() > 500, "Native editor constructs");
    if (argc > 1) {
        juce::FileOutputStream output(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])); juce::PNGImageFormat png;
        check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), output), "Editor preview renders");
    }
    std::cout << "PASS: native MIDI sample timing, project recall, matching audio, panic, on-screen keyboard, ten audible patches, and editor rendering\n";
}
