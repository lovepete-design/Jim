#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchEngine.h"
#include "ManualClip.h"

class TriadProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    TriadProcessor();
    ~TriadProcessor() override { stopTimer(); }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Triad Tune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> detected { -1 }, target { -1 }, confidence { 0 }, level { 0 };
    triad::ManualClip clip;
    std::atomic<double> playheadSeconds {0};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    triad::Engine engine;
    // Cache raw parameter pointers so the callback does not perform tree lookups.
    std::array<std::atomic<float>*, 9> values {};
    std::atomic<int> liveLatency {992};
    void timerCallback() override;
    void render(juce::AudioBuffer<float>&, bool bypassed);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriadProcessor)
};
