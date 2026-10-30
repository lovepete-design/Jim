#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <mutex>
#if TRIAD_KIND == 0
#include "../plugins/hex/HexEngine.h"
#elif TRIAD_KIND == 1
#include "../plugins/stress/Dynamics.h"
#else
#include "../plugins/lattice/Lattice.h"
#endif

class TriadProcessor final : public juce::AudioProcessor, private juce::MidiKeyboardState::Listener
{
public:
    TriadProcessor();
    ~TriadProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return title(); }
    bool acceptsMidi() const override { return TRIAD_KIND != 1; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return TRIAD_KIND == 1 ? 0.0 : 20.0; }
    int getNumPrograms() override { return presetNames().size(); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram(int index) override { selectPreset(index); }
    const juce::String getProgramName(int index) override { return presetNames()[index]; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::String title() const;
    juce::String subtitle() const;
    juce::Colour accent() const;
    float meter() const noexcept { return meterValue.load(std::memory_order_relaxed); }
    juce::StringArray presetNames() const;
    void selectPreset(int);
    juce::Result loadSysexFile(const juce::File&);
#if TRIAD_KIND == 0
    void loadLibraryVoice(const std::array<unsigned char,155>&, const juce::String& name, const juce::String& id);
    juce::String loadedVoiceName() const;
    juce::String loadedVoiceId() const;
#endif
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEngineParameters();
    void handleMidi(const juce::MidiMessage&);
    void render(juce::AudioBuffer<float>&, int start, int length);
    float raw(size_t i) const noexcept { return atoms[i]->load(std::memory_order_relaxed); }
    void setValue(const char*, float);
    void handleNoteOn(juce::MidiKeyboardState*, int, int, float) override;
    void handleNoteOff(juce::MidiKeyboardState*, int, int, float) override;
    void queueKeyboard(int note, int velocity);
    struct KeyboardEvent { int note=0, velocity=0; };
    juce::AbstractFifo keyboardQueue{128};
    std::array<KeyboardEvent,128> keyboardEvents{};
    std::atomic<bool> keyboardOverflow{false};
    std::array<std::atomic<float>*, 12> atoms{};
    std::atomic<float> meterValue{0};
    std::atomic<int> currentProgram{0};
    std::atomic<bool> resetRequested{false};
#if TRIAD_KIND == 0
    triad::HexEngine engine;
    using Patch = std::array<unsigned char, 155>;
    // Triple buffer: the producer and audio thread always own distinct slots.
    std::array<Patch, 3> patches{};
    std::atomic<int> patchMiddle{1};
    int patchFront=0, patchBack=2;
    Patch savedPatch{};
    mutable std::recursive_mutex patchProducerMutex;
    juce::String savedVoiceName{"Glass tine"}, savedVoiceId;
    bool queuePatch(const Patch&);
#elif TRIAD_KIND == 1
    triad::Dynamics engine;
#else
    triad::Lattice engine;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriadProcessor)
};
