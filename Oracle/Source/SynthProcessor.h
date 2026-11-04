#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SynthDSP.h"
#include "Parameters.h"
class OracleProcessor final : public juce::AudioProcessor {
public:
    OracleProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override { engine.panic(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Triad Oracle"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 9; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void applyPatch(int);
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::atomic<bool> panicRequested {false};
    std::atomic<int> voiceCount {0};
    std::atomic<float> outputPeak {0};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters();
    void updateSettings();
    void event(const juce::MidiMessage&);
    oracle::Engine engine;
    std::array<std::atomic<float>*, 25> values {};
    std::atomic<float> *waveA = nullptr, *waveB = nullptr, *sync = nullptr, *keytrack = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OracleProcessor)
};
