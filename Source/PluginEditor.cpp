#include "PluginEditor.h"
namespace {
const juce::Colour background {0xff10141c}, panel {0xff191f2b}, muted {0xff8390a4}, accent {0xff7be5cd};
juce::String noteName(float midi) {
    if (midi < 0) return "--";
    static const char* names[] {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int note = juce::roundToInt(midi);
    return juce::String(names[(note % 12 + 12) % 12]) + juce::String(note / 12 - 1);
}
void text(juce::Graphics& g, juce::String label, juce::Rectangle<int> bounds, float size, juce::Colour colour,
          juce::Justification alignment = juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawText(label, bounds, alignment);
}
}
TriadLook::TriadLook() {
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff303b4d));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff304c50));
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, muted);
    setColour(juce::ToggleButton::tickColourId, accent);
}
void TriadLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                float position, float start, float end, juce::Slider&) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(width), static_cast<float>(height)).reduced(12);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2;
    const auto centre = bounds.getCentre();
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
    value.addCentredArc(centre.x, centre.y, radius, radius, 0, start, start + position * (end - start), true);
    g.setColour(juce::Colour(0xff303b4d)); g.strokePath(track, juce::PathStrokeType(4));
    g.setColour(accent); g.strokePath(value, juce::PathStrokeType(4));
    g.setColour(juce::Colour(0xff222b3a)); g.fillEllipse(centre.x - radius + 9, centre.y - radius + 9, 2 * (radius - 9), 2 * (radius - 9));
    const float angle = start + position * (end - start);
    g.setColour(juce::Colours::white);
    g.drawLine(centre.x + std::sin(angle) * (radius - 20), centre.y - std::cos(angle) * (radius - 20),
               centre.x + std::sin(angle) * (radius - 12), centre.y - std::cos(angle) * (radius - 12), 2);
}
TriadEditor::TriadEditor(TriadProcessor& p) : AudioProcessorEditor(p), processor(p), noteGrid(p) {
    setLookAndFeel(&look); setSize(860, 780);
    root.addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scale.addItemList({"Chromatic", "Major", "Natural minor", "Major pentatonic"}, 1);
    preset.addItemList({"Natural vocal", "Tight pop", "Hard tune"}, 1);
    preset.setTextWhenNothingSelected("Starting points");
    preset.onChange = [this] { applyPreset(preset.getSelectedId()); };
    addAndMakeVisible(root); addAndMakeVisible(scale); addAndMakeVisible(preset); addAndMakeVisible(bypass);
    mode.addItemList({"Note editor", "Live tune"}, 1);
    addAndMakeVisible(mode); addAndMakeVisible(transfer); addAndMakeVisible(finish); addAndMakeVisible(noteGrid);
    transfer.onClick = [this] { processor.clip.arm(); };
    finish.onClick = [this] { processor.clip.stop(); };
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "mode", mode);
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "root", root);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "scale", scale);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "bypass", bypass);
    const char* ids[] {"retune", "strength", "humanize", "mix", "output"};
    const char* suffix[] {" ms", " %", " ct", " %", " dB"};
    const char* tooltips[] {"Time to approach the selected note. Zero gives a hard tune effect.",
        "How much of the pitch error to correct.", "Pitch variation inside this distance is left alone.",
        "Blend the corrected vocal with the delayed original.", "Output gain. Lower this if your DAW shows clipping."};
    for (size_t i = 0; i < knobs.size(); ++i) {
        auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 24);
        knob.setTextValueSuffix(suffix[i]); knob.setTooltip(tooltips[i]);
        addAndMakeVisible(knob);
        sliders[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, ids[i], knob);
    }
    pitchHistory.fill(-1); targetHistory.fill(-1); startTimerHz(30);
}
TriadEditor::~TriadEditor() { stopTimer(); setLookAndFeel(nullptr); }
void TriadEditor::applyPreset(int id) {
    const std::array<std::array<float, 3>, 3> presets {{{65, 80, 12}, {18, 100, 5}, {0, 100, 0}}};
    if (id < 1 || id > 3) return;
    const char* ids[] {"retune", "strength", "humanize"};
    for (size_t i = 0; i < 3; ++i) {
        auto* parameter = processor.parameters.getParameter(ids[i]);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(presets[static_cast<size_t>(id - 1)][i]));
        parameter->endChangeGesture();
    }
}
void TriadEditor::resized() {
    preset.setBounds(535, 28, 175, 34); bypass.setBounds(732, 28, 100, 34);
    root.setBounds(54, 128, 130, 36); scale.setBounds(204, 128, 220, 36);
    mode.setBounds(54, 180, 190, 32); transfer.setBounds(264, 180, 120, 32); finish.setBounds(394, 180, 100, 32);
    noteGrid.setBounds(54, 225, 752, 300);
    for (size_t i = 0; i < knobs.size(); ++i) knobs[i].setBounds(42 + static_cast<int>(i) * 157, 595, 148, 130);
}
void TriadEditor::timerCallback() {
    pitchHistory[cursor] = processor.detected.load(std::memory_order_relaxed);
    targetHistory[cursor] = processor.target.load(std::memory_order_relaxed);
    cursor = (cursor + 1) % pitchHistory.size(); repaint();
    const bool manual = processor.parameters.getRawParameterValue("mode")->load() < 0.5f;
    noteGrid.setVisible(manual); transfer.setVisible(manual); finish.setVisible(manual);
    const int state = processor.clip.state.load();
    transfer.setEnabled(state == triad::ManualClip::Empty || state == triad::ManualClip::Ready);
    finish.setEnabled(state == triad::ManualClip::Armed || state == triad::ManualClip::Recording);
    for (auto& knob : knobs) knob.setEnabled(!manual);
    root.setEnabled(!manual); scale.setEnabled(!manual); preset.setEnabled(!manual);
    noteGrid.repaint();
}
void TriadEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    text(g, "TRIAD", {30, 18, 170, 32}, 25, accent);
    text(g, "TUNE / VOCAL PITCH CORRECTION", {31, 51, 360, 20}, 11, muted);
    g.setColour(panel); g.fillRoundedRectangle(30, 92, 800, 464, 14);
    text(g, "KEY", {54, 103, 100, 20}, 11, muted);
    text(g, "SCALE", {204, 103, 180, 20}, 11, muted);
    const auto input = processor.detected.load(); const auto destination = processor.target.load();
    text(g, "INPUT", {470, 103, 80, 20}, 11, muted);
    text(g, noteName(input), {470, 126, 100, 35}, 29, juce::Colours::white);
    text(g, "TARGET", {590, 103, 80, 20}, 11, muted);
    text(g, noteName(destination), {590, 126, 100, 35}, 29, accent);
    text(g, input < 0 ? "LISTENING" : "TRACKING", {701, 136, 100, 20}, 11, input < 0 ? muted : accent);
    const juce::Rectangle<float> chart(54, 225, 752, 300);
    g.saveState(); g.reduceClipRegion(chart.toNearestInt());
    const float centre = destination >= 0 ? destination : 60;
    for (int semitone = -5; semitone <= 5; ++semitone) {
        const float y = chart.getCentreY() - semitone * 15;
        g.setColour(juce::Colour(0xff283141)); g.drawHorizontalLine(static_cast<int>(y), chart.getX(), chart.getRight());
    }
    auto drawHistory = [&](const auto& history, juce::Colour colour, float width) {
        juce::Path path; bool penDown = false;
        for (size_t i = 0; i < history.size(); ++i) {
            const float value = history[(cursor + i) % history.size()];
            if (value < 0) { penDown = false; continue; }
            const float x = chart.getX() + static_cast<float>(i) / static_cast<float>(history.size() - 1) * chart.getWidth();
            const float y = chart.getCentreY() - (value - centre) * 15;
            if (!penDown) path.startNewSubPath(x, y); else path.lineTo(x, y);
            penDown = true;
        }
        g.setColour(colour); g.strokePath(path, juce::PathStrokeType(width));
    };
    drawHistory(targetHistory, accent.withAlpha(0.3f), 1.5f); drawHistory(pitchHistory, accent, 2);
    if (input < 0) text(g, "Sing a single vocal line to see its pitch", chart.toNearestInt(), 14, muted, juce::Justification::centred);
    g.restoreState();
    const char* labels[] {"RETUNE SPEED", "CORRECTION", "TOLERANCE", "DRY / WET", "OUTPUT"};
    for (int i = 0; i < 5; ++i) text(g, labels[i], {42 + i * 157, 577, 148, 22}, 11, muted, juce::Justification::centred);
    const bool manual = processor.parameters.getRawParameterValue("mode")->load() < 0.5f;
    if (manual) {
        const char* status[] {"Transfer, then play your vocal", "Armed - start Ableton playback", "Recording - click Finish when done",
            "Finishing transfer...", "Preparing analysis...", "Detecting notes...", "Drag notes / double-click to reset"};
        const int s = std::clamp(processor.clip.state.load(), 0, 6);
        text(g, status[s], {510, 180, 296, 32}, 12, accent);
        text(g, "Wheel: zoom  /  Shift + wheel: pan  /  Edits: +/-12 semitones", {54, 530, 750, 20}, 11, muted);
    }
    g.setColour(juce::Colour(0xff293242)); g.drawHorizontalLine(744, 30, 830);
    text(g, manual ? "NOTE EDITOR  /  0 SAMPLES ADDED DELAY" : "LIVE TUNE  /  APPROX. 21 ms ADDED DELAY", {31, 750, 500, 20}, 11, muted);
    text(g, "TRIAD AUDIO     v0.1", {620, 750, 210, 20}, 11, muted, juce::Justification::centredRight);
}
