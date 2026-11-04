#include "PhaserProcessor.h"
#include <iostream>
void check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
struct Host final : juce::AudioPlayHead {
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm(100); return p; }
};
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    PhaserProcessor original; original.prepareToPlay(48000, 256);
    check(original.getLatencySamples() == 0, "Host latency is zero");
    auto set = [&](const char* id, float value) { auto* p = original.state.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(value)); };
    set("mode", 1); set("feedback", 48); set("sync", 1); set("division", 3);
    juce::MemoryBlock data; original.getStateInformation(data);
    PhaserProcessor restored; restored.setStateInformation(data.getData(), static_cast<int>(data.getSize())); restored.prepareToPlay(48000, 256);
    check(std::abs(restored.state.getRawParameterValue("feedback")->load() - 48) < 0.01f, "Resonance survives project recall");
    check(restored.state.getRawParameterValue("sync")->load() > 0.5f, "Tempo-sync switch survives recall");
    Host host; original.setPlayHead(&host); restored.setPlayHead(&host); juce::MidiBuffer midi;
    for (int block = 0; block < 200; ++block) {
        juce::AudioBuffer<float> a(2, 256), b(2, 256);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) {
            const float value = static_cast<float>(0.2 * std::sin(2 * triadphase::pi * 440 * (block * 256 + i) / 48000));
            a.setSample(ch, i, value); b.setSample(ch, i, value);
        }
        original.processBlock(a, midi); restored.processBlock(b, midi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i)
            check(std::abs(a.getSample(ch, i) - b.getSample(ch, i)) < 1e-7f, "Recalled processor renders identically with host tempo");
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(original.createEditor());
    check(editor && editor->getWidth() > 500, "Native editor constructs");
    if (argc > 1) {
        juce::FileOutputStream output(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
        juce::PNGImageFormat png;
        check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), output), "Editor screenshot renders");
    }
    std::cout << "PASS: native processor recall, tempo input, zero latency, and editor construction\n";
}
