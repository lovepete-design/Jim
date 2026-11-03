#include "PluginProcessor.h"
#include <chrono>
#include <iostream>

void check(bool ok, const char* description) {
    if (!ok) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
struct Transport final : juce::AudioPlayHead {
    double seconds = 0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p; p.setTimeInSeconds(seconds); p.setIsPlaying(playing); return p;
    }
};
int main() {
    juce::ScopedJuceInitialiser_GUI initialiser;
    TriadProcessor original;
    Transport transport; original.setPlayHead(&transport);
    original.prepareToPlay(48000, 256);
    check(original.getLatencySamples() == 0, "Editor reports zero latency to host");
    original.clip.arm();
    juce::AudioBuffer<float> buffer(2, 256); juce::MidiBuffer midi;
    for (int block = 0; block < 200; ++block) {
        transport.seconds = block * 256 / 48000.;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i) buffer.setSample(ch, i, static_cast<float>(0.2 * std::sin(2 * triad::pi * 220 * (block * 256 + i) / 48000)));
        original.processBlock(buffer, midi);
    }
    original.clip.stop(); transport.playing = false; original.processBlock(buffer, midi); original.clip.service();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!original.clip.ready() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    check(original.clip.ready() && original.clip.countNotes() > 0, "Processor captures host-synchronised notes");
    original.clip.transpose(0, 7);
    juce::MemoryBlock state; original.getStateInformation(state);
    TriadProcessor recalled; recalled.prepareToPlay(48000, 256);
    recalled.setPlayHead(&transport); recalled.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(recalled.clip.ready() && recalled.clip.note(0).transpose.load() == 7, "Serialized plugin state restores capture and edits");
    transport.playing = true; transport.seconds = 0.5;
    juce::AudioBuffer<float> a(2, 256), b(2, 256); a.clear(); b.clear();
    original.processBlock(a, midi); recalled.processBlock(b, midi);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i)
        check(a.getSample(ch, i) == b.getSample(ch, i), "Recalled processor reproduces edited playback exactly");
    check(a.getMagnitude(0, 256) > 0.01f, "Edited playback is audible");
    recalled.prepareToPlay(96000, 256);
    check(recalled.clip.ready() && recalled.clip.note(0).transpose.load() == 7, "Host prepare preserves manual edits");
    auto* mode = recalled.parameters.getParameter("mode"); mode->setValueNotifyingHost(1);
    recalled.prepareToPlay(48000, 256);
    check(recalled.getLatencySamples() == 992, "Live mode advertises nominal delay");
    std::cout << "PASS: native processor transport capture, binary state recall, sample rate changes, and latency reporting\n";
}
