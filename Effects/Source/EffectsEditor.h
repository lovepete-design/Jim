#pragma once
#include "EffectsProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
class EffectsLook final : public juce::LookAndFeel_V4 {
public:
    EffectsLook();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
class EffectsEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit EffectsEditor(EffectsProcessor&);
    ~EffectsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    EffectsProcessor& processor;
    EffectsLook look;
    std::array<juce::Slider, triadfx::parameters.size()> knobs;
    std::array<juce::ToggleButton, triadfx::flagIds.size()> buttons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, triadfx::parameters.size()> attachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, triadfx::flagIds.size()> buttonAttachments;
    juce::ComboBox presets;
    juce::TooltipWindow tooltip {this, 500};
    std::array<float, 6> displayed {-90, -90, -90, 0, 0, -90};
    std::array<float, 220> reductionHistory {};
    size_t cursor = 0;
    void timerCallback() override;
    void applyPreset(int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsEditor)
};
