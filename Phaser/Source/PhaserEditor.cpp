#include "PhaserEditor.h"
namespace {
const juce::Colour background(0xff151311), panel(0xff231d18), orange(0xffffa24e), muted(0xffb1a294), white(0xfffff0e2);
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> area, float size, juce::Colour colour,
          juce::Justification align = juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawText(value, area, align);
}
}
PhaseLook::PhaseLook() {
    setColour(juce::ComboBox::backgroundColourId, panel); setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff57432f));
    setColour(juce::ComboBox::textColourId, white); setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff57432f));
    setColour(juce::Slider::textBoxTextColourId, white); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, muted); setColour(juce::ToggleButton::tickColourId, orange);
}
void PhaseLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float start, float end, juce::Slider&) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(12);
    const auto centre = bounds.getCentre(); const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
    value.addCentredArc(centre.x, centre.y, radius, radius, 0, start, start + position * (end - start), true);
    g.setColour(juce::Colour(0xff59422e)); g.strokePath(track, juce::PathStrokeType(4));
    g.setColour(orange); g.strokePath(value, juce::PathStrokeType(4));
    g.setColour(juce::Colour(0xff34302a)); g.fillEllipse(centre.x - radius + 8, centre.y - radius + 8, 2 * (radius - 8), 2 * (radius - 8));
    const float angle = start + position * (end - start);
    g.setColour(white); g.drawLine(centre.x + std::sin(angle) * (radius - 23), centre.y - std::cos(angle) * (radius - 23),
        centre.x + std::sin(angle) * (radius - 11), centre.y - std::cos(angle) * (radius - 11), 3);
}
PhaserEditor::PhaserEditor(PhaserProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&look); setSize(880, 650);
    presets.addItemList({"Classic slow sweep", "Resonant swirl", "Gentle two-stage", "Fast swirl", "Wide stereo"}, 1);
    presets.setTextWhenNothingSelected("Starting points"); presets.onChange = [this] { preset(presets.getSelectedId()); };
    mode.addItemList({"Smooth", "Resonant"}, 1); stages.addItemList({"4 stages", "2 stages"}, 1);
    division.addItemList({"1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars"}, 1);
    for (auto* c : {&presets, &mode, &stages, &division}) addAndMakeVisible(c);
    addAndMakeVisible(sync); addAndMakeVisible(bypass);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "mode", mode);
    stagesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "stages", stages);
    divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "division", division);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "sync", sync);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "bypass", bypass);
    const char* help[] {"Sweep speed. Tempo Sync uses the host BPM instead.", "Sweep excursion around the centre frequency.",
        "Feedback amount in Resonant mode; Smooth mode has no feedback.", "Equal dry and wet gives the classic cancellation notches.",
        "Offsets the right-channel sweep. Zero keeps the classic mono-compatible sweep.", "Output trim; resonance can increase peak levels."};
    for (size_t i = 0; i < knobs.size(); ++i) {
        const auto& c = phaseControls[i]; auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 125, 25);
        knob.setTextValueSuffix(c.suffix); knob.setTooltip(help[i]); knob.setDoubleClickReturnValue(true, c.initial);
        addAndMakeVisible(knob); attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, c.id, knob);
    }
    startTimerHz(30);
}
PhaserEditor::~PhaserEditor() { stopTimer(); setLookAndFeel(nullptr); }
void PhaserEditor::resized() {
    presets.setBounds(607, 26, 242, 34);
    mode.setBounds(36, 98, 170, 34); stages.setBounds(224, 98, 150, 34); bypass.setBounds(493, 100, 115, 30);
    const int order[] {0, 1, 3, 2, 4, 5};
    for (int i = 0; i < 6; ++i) knobs[static_cast<size_t>(order[i])].setBounds(33 + (i % 3) * 194, 178 + (i / 3) * 195, 180, 155);
    sync.setBounds(650, 383, 180, 30); division.setBounds(650, 425, 163, 34);
}
void PhaserEditor::timerCallback() {
    peak = std::max(processor.outputPeak.load(), peak * 0.93f);
    const bool synced = processor.state.getRawParameterValue("sync")->load() > 0.5f;
    knobs[0].setEnabled(!synced); division.setEnabled(synced);
    knobs[2].setEnabled(processor.state.getRawParameterValue("mode")->load() > 0.5f);
    repaint();
}
void PhaserEditor::preset(int id) {
    if (id < 1) return;
    auto set = [this](const char* key, float value) {
        if (auto* p = processor.state.getParameter(key)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture(); }
    };
    for (const auto& c : phaseControls) set(c.id, c.initial);
    set("mode", 0); set("stages", 0); set("sync", 0); set("division", 2); set("bypass", 0);
    if (id == 2) { set("mode", 1); set("feedback", 45); set("rate", 0.7f); }
    if (id == 3) { set("stages", 1); set("rate", 0.4f); set("depth", 80); }
    if (id == 4) { set("rate", 4); set("depth", 85); }
    if (id == 5) { set("stereo", 120); set("rate", 0.3f); }
}
void PhaserEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    text(g, "TRIAD PHASE", {32, 20, 530, 38}, 30, orange);
    text(g, "VINTAGE-INSPIRED ANALOGUE SWEEP", {34, 59, 520, 18}, 11, muted);
    g.setColour(panel); g.fillRoundedRectangle(24, 148, 596, 437, 14); g.fillRoundedRectangle(636, 148, 220, 437, 14);
    const int order[] {0, 1, 3, 2, 4, 5};
    for (int i = 0; i < 6; ++i)
        text(g, phaseControls[static_cast<size_t>(order[i])].label, {33 + (i % 3) * 194, 159 + (i / 3) * 195, 180, 24}, 11, muted, juce::Justification::centred);
    text(g, "SWEEP", {652, 169, 170, 22}, 11, muted);
    const float lfo = std::clamp(processor.sweep.load(), 0.f, 1.f);
    g.setColour(juce::Colour(0xff59422e)); g.fillRoundedRectangle(656, 217, 176, 6, 3);
    g.setColour(orange); g.fillEllipse(650 + lfo * 176, 211, 18, 18);
    text(g, juce::String(processor.leftFrequency.load(), 0) + " Hz", {652, 248, 170, 35}, 27, white);
    text(g, "ALL-PASS CORNER", {652, 285, 190, 18}, 10, muted);
    text(g, "OUTPUT PEAK", {652, 316, 170, 18}, 10, muted);
    const float db = 20 * std::log10(std::max(1e-9f, peak));
    text(g, (db < -90 ? juce::String("--") : juce::String(db, 1)) + " dBFS", {652, 338, 180, 30}, 22, db >= 0 ? juce::Colours::orangered : white);
    text(g, "HOST TEMPO", {652, 482, 170, 18}, 10, muted);
    text(g, "Free-running sweep phase", {652, 505, 190, 20}, 11, muted);
    text(g, "120 BPM fallback without host", {652, 528, 190, 20}, 10, muted);
    g.setColour(juce::Colour(0xff57432f)); g.drawHorizontalLine(607, 30, 850);
    text(g, "ZERO ADDED LATENCY  /  MONO + STEREO", {31, 617, 580, 20}, 10, muted);
    text(g, "TRIAD AUDIO   v0.1", {626, 617, 223, 20}, 10, muted, juce::Justification::centredRight);
}
