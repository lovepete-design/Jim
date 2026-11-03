#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioDSP.h"
#include "Parameters.h"
class EffectsProcessor final : public juce::AudioProcessor {
public:
    EffectsProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return triadfx::productName; }
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
    std::array<std::atomic<float>, 6> meters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    std::array<std::atomic<float>*, triadfx::parameters.size()> controls {};
    std::array<std::atomic<float>*, triadfx::flagIds.size()> flags {};
#if TRIAD_MASTERING
    triadfx::Mastering engine;
#else
    triadfx::DeEsser engine;
#endif
    void render(juce::AudioBuffer<float>&, bool);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsProcessor)
};
