#pragma once
#include "SynthProcessor.h"
class OracleLook final : public juce::LookAndFeel_V4 {
public:
    OracleLook();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
class OracleEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OracleEditor(OracleProcessor&);
    ~OracleEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    OracleProcessor& processor;
    OracleLook look;
    juce::MidiKeyboardComponent piano;
    std::array<juce::Slider, 25> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 25> attachments;
    juce::ComboBox patches, waveA, waveB;
    juce::ToggleButton sync {"OSC SYNC"}, keytrack {"KEY TRACK"};
    juce::TextButton panic {"PANIC"};
    std::array<juce::TextButton, 3> tabs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAAttachment, waveBAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, trackAttachment;
    juce::TooltipWindow tooltip {this, 500};
    int page = 0;
    float peak = 0;
    void choosePage(int);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OracleEditor)
};
