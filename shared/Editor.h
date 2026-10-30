#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class TriadProcessor;

class TriadEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TriadEditor (TriadProcessor&);
    ~TriadEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    struct Content;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriadEditor)
};
