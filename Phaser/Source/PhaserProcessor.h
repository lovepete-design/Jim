#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PhaserDSP.h"
struct PhaseControl { const char* id; const char* label; float min, max, initial, step, skew; const char* suffix; };
inline constexpr std::array<PhaseControl, 6> phaseControls {{
    {"rate", "RATE", 0.05f, 10, 0.45f, 0.01f, 0.4f, " Hz"},
    {"depth", "DEPTH", 0, 100, 100, 0.1f, 1, " %"},
    {"feedback", "RESONANCE", 0, 70, 35, 0.1f, 1, " %"},
    {"mix", "DRY / WET", 0, 100, 50, 0.1f, 1, " %"},
    {"stereo", "STEREO SPREAD", 0, 180, 0, 1, 1, " deg"},
    {"output", "OUTPUT", -18, 12, 0, 0.1f, 1, " dB"}
}};
class PhaserProcessor final : public juce::AudioProcessor {
public:
    PhaserProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Triad Phase"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> sweep {0.5f}, outputPeak {0}, leftFrequency {500};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters();
    std::array<std::atomic<float>*, 6> values {};
    std::atomic<float> *mode = nullptr, *stageCount = nullptr, *sync = nullptr, *division = nullptr, *bypass = nullptr;
    triadphase::Engine engine;
    void render(juce::AudioBuffer<float>&, bool);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaserProcessor)
};
