#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "NoteGrid.h"

class TriadLook final : public juce::LookAndFeel_V4 {
public:
    TriadLook();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
class TriadEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TriadEditor(TriadProcessor&);
    ~TriadEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    TriadProcessor& processor;
    TriadLook look;
    juce::ComboBox root, scale, preset, mode;
    juce::TextButton transfer {"TRANSFER"}, finish {"FINISH"};
    NoteGrid noteGrid;
    juce::ToggleButton bypass {"BYPASS"};
    std::array<juce::Slider, 5> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> sliders;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::array<float, 360> pitchHistory {}, targetHistory {};
    size_t cursor = 0;
    void timerCallback() override;
    void applyPreset(int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriadEditor)
};
