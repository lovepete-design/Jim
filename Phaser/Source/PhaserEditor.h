#pragma once
#include "PhaserProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
class PhaseLook final : public juce::LookAndFeel_V4 {
public:
    PhaseLook();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
class PhaserEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PhaserEditor(PhaserProcessor&);
    ~PhaserEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    PhaserProcessor& processor;
    PhaseLook look;
    std::array<juce::Slider, 6> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> attachments;
    juce::ComboBox presets, mode, stages, division;
    juce::ToggleButton sync {"TEMPO SYNC"}, bypass {"BYPASS"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment, stagesAttachment, divisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, bypassAttachment;
    juce::TooltipWindow tooltip {this, 500};
    float peak = 0;
    void timerCallback() override;
    void preset(int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaserEditor)
};
