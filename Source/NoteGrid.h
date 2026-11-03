#pragma once
#include "PluginProcessor.h"

class NoteGrid final : public juce::Component {
public:
    explicit NoteGrid(TriadProcessor& p) : processor(p) {}
    void paint(juce::Graphics& g) override {
        const juce::Colour accent(0xff7be5cd), muted(0xff8390a4);
        g.fillAll(juce::Colour(0xff141a24));
        const int rowHeight = getHeight() / rows;
        const float row = static_cast<float>(getHeight()) / rows;
        for (int i = 0; i < rows; ++i) {
            const int midi = highest - i;
            const int pc = midi % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            g.setColour(black ? juce::Colour(0xff141a24) : juce::Colour(0xff1a2230));
            g.fillRect(0.f, i * row, static_cast<float>(getWidth()), row);
            if (pc == 0) {
                g.setColour(muted); g.setFont(juce::FontOptions(10));
                g.drawText("C" + juce::String(midi / 12 - 1), 2, static_cast<int>(i * row), 30, std::max(10, rowHeight), juce::Justification::centredLeft);
            }
        }
        if (!processor.clip.ready()) {
            g.setColour(muted); g.setFont(juce::FontOptions(15));
            g.drawText("Transfer a vocal passage from Ableton to edit its notes", getLocalBounds(), juce::Justification::centred);
            return;
        }
        for (int sec = static_cast<int>(viewStart); sec <= static_cast<int>(viewStart + viewSpan); ++sec) {
            const float x = xFor(sec); g.setColour(juce::Colour(0xff303b4d)); g.drawVerticalLine(static_cast<int>(x), 0, static_cast<float>(getHeight()));
        }
        g.saveState(); g.reduceClipRegion(34, 0, getWidth() - 34, getHeight());
        for (int i = 0; i < processor.clip.countNotes(); ++i) {
            auto bounds = boundsFor(i);
            if (!bounds.intersects(getLocalBounds().toFloat())) continue;
            g.setColour(i == selected ? juce::Colour(0xffb4ffee) : accent.withAlpha(0.75f));
            g.fillRoundedRectangle(bounds, 2);
        }
        const double relative = processor.playheadSeconds.load() - clipOrigin();
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine(static_cast<int>(xFor(relative)), 0, static_cast<float>(getHeight()));
        g.restoreState();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        selected = -1;
        for (int i = 0; i < processor.clip.countNotes(); ++i) {
            if (boundsFor(i).expanded(0, 2).contains(e.position)) {
                selected = i; originalShift = processor.clip.note(i).transpose.load(); break;
            }
        }
        dragStartY = e.y; repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (selected < 0) return;
        const int difference = juce::roundToInt((dragStartY - e.y) * static_cast<float>(rows) / getHeight());
        processor.clip.transpose(selected, originalShift + difference); repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (selected >= 0) processor.clip.reset(selected);
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (e.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY)) {
            viewStart -= (wheel.deltaX + wheel.deltaY) * viewSpan;
        } else {
            const double fraction = std::clamp((e.x - 34.0) / std::max(1, getWidth() - 34), 0.0, 1.0);
            const double anchor = viewStart + fraction * viewSpan;
            viewSpan = std::clamp(viewSpan * std::exp(-wheel.deltaY * 2), 1.0, 120.0);
            viewStart = anchor - fraction * viewSpan;
        }
        viewStart = std::clamp(viewStart, 0.0, std::max(0.0, processor.clip.duration() - viewSpan)); repaint();
    }
private:
    TriadProcessor& processor;
    static constexpr int highest = 84, rows = 49;
    int selected = -1, originalShift = 0, dragStartY = 0;
    double viewStart = 0, viewSpan = 12;
    float xFor(double seconds) const { return static_cast<float>(34 + (seconds - viewStart) / viewSpan * (getWidth() - 34)); }
    double clipOrigin() const { return processor.clip.startTime(); }
    juce::Rectangle<float> boundsFor(int i) const {
        const auto& n = processor.clip.note(i);
        const float row = static_cast<float>(getHeight()) / rows;
        const float pitch = static_cast<float>(std::round(n.original)) + n.transpose.load();
        const float left = xFor(n.start / processor.clip.rate()), right = xFor(n.end / processor.clip.rate());
        return {left, (highest - pitch) * row + 0.5f, std::max(3.f, right - left - 1), std::max(3.f, row - 1)};
    }
};
