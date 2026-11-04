#include "SynthEditor.h"
namespace {
const juce::Colour background(0xff111316), panel(0xff1c2027), wood(0xff473126), gold(0xffe6bf79), muted(0xff9ba3b1), white(0xfff0eadd);
const std::array<std::vector<int>, 3> pages {{{0, 1, 2, 3, 4, 24, 5, 6, 7}, {8, 9, 10, 11, 12, 13, 14, 15}, {16, 17, 18, 19, 20, 23, 21, 22}}};
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> bounds, float size, juce::Colour colour,
          juce::Justification alignment = juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawText(value, bounds, alignment);
}
}
OracleLook::OracleLook() {
    setColour(juce::ComboBox::backgroundColourId, panel); setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff4a4942));
    setColour(juce::ComboBox::textColourId, white); setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff4a4942));
    setColour(juce::Slider::textBoxTextColourId, white); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, muted); setColour(juce::ToggleButton::tickColourId, gold);
    setColour(juce::TextButton::buttonColourId, panel); setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff615136));
}
void OracleLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float value, float start, float end, juce::Slider&) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(8);
    const auto centre = bounds.getCentre(); const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    juce::Path track, filled; track.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
    filled.addCentredArc(centre.x, centre.y, radius, radius, 0, start, start + value * (end - start), true);
    g.setColour(juce::Colour(0xff41454e)); g.strokePath(track, juce::PathStrokeType(3));
    g.setColour(gold); g.strokePath(filled, juce::PathStrokeType(3));
    g.setColour(juce::Colour(0xff30353e)); g.fillEllipse(centre.x - radius + 5, centre.y - radius + 5, 2 * (radius - 5), 2 * (radius - 5));
    const float angle = start + value * (end - start);
    g.setColour(white); g.drawLine(centre.x + std::sin(angle) * (radius - 18), centre.y - std::cos(angle) * (radius - 18),
        centre.x + std::sin(angle) * (radius - 8), centre.y - std::cos(angle) * (radius - 8), 2);
}
OracleEditor::OracleEditor(OracleProcessor& p) : AudioProcessorEditor(p), processor(p), piano(p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&look); setSize(1000, 720);
    for (size_t i = 0; i < patchNames.size(); ++i) patches.addItem(patchNames[i], static_cast<int>(i) + 1);
    patches.setTextWhenNothingSelected("Warm Brass / starting points"); patches.onChange = [this] { processor.applyPatch(patches.getSelectedId() - 1); };
    waveA.addItemList({"A: Saw", "A: Pulse", "A: Triangle"}, 1); waveB.addItemList({"B: Saw", "B: Pulse", "B: Triangle"}, 1);
    for (auto* component : {&patches, &waveA, &waveB}) addAndMakeVisible(component);
    addAndMakeVisible(sync); addAndMakeVisible(keytrack); addAndMakeVisible(panic); addAndMakeVisible(piano);
    panic.onClick = [this] { processor.keyboard.reset(); processor.panicRequested.store(true); };
    waveAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "wavea", waveA);
    waveBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "waveb", waveB);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "sync", sync);
    trackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "keytrack", keytrack);
    const char* names[] {"VOICE & FILTER", "ENVELOPES", "MODULATION & FX"};
    for (int i = 0; i < 3; ++i) {
        tabs[static_cast<size_t>(i)].setButtonText(names[i]); tabs[static_cast<size_t>(i)].setClickingTogglesState(true);
        tabs[static_cast<size_t>(i)].onClick = [this, i] { choosePage(i); }; addAndMakeVisible(tabs[static_cast<size_t>(i)]);
    }
    for (size_t i = 0; i < knobs.size(); ++i) {
        const auto& spec = oracleControls[i]; auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 150, 23);
        knob.setTextValueSuffix(spec.suffix); knob.setDoubleClickReturnValue(true, spec.initial); knob.setName(spec.label); addChildComponent(knob);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, spec.id, knob);
    }
    knobs[19].setTooltip("Oscillator B waveform modulates A's pitch. High amounts make bright or metallic sounds.");
    knobs[20].setTooltip("Filter envelope modulates A's pitch. Useful for sync sweeps and percussive tones.");
    knobs[0].setTooltip("Oscillator balance. Left is A only, right is B only.");
    piano.setAvailableRange(36, 96); piano.setLowestVisibleKey(48); piano.setKeyPressBaseOctave(3);
    piano.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, gold.withAlpha(0.7f));
    choosePage(0); startTimerHz(30);
}
OracleEditor::~OracleEditor() { stopTimer(); setLookAndFeel(nullptr); }
void OracleEditor::choosePage(int value) {
    page = value;
    for (auto& knob : knobs) knob.setVisible(false);
    for (int index : pages[static_cast<size_t>(page)]) knobs[static_cast<size_t>(index)].setVisible(true);
    for (size_t i = 0; i < tabs.size(); ++i) tabs[i].setToggleState(static_cast<int>(i) == page, juce::dontSendNotification);
    resized(); repaint();
}
void OracleEditor::resized() {
    patches.setBounds(624, 25, 255, 36); panic.setBounds(895, 25, 76, 36);
    waveA.setBounds(32, 95, 190, 34); waveB.setBounds(238, 95, 190, 34); sync.setBounds(456, 97, 145, 30); keytrack.setBounds(620, 97, 170, 30);
    for (size_t i = 0; i < tabs.size(); ++i) tabs[i].setBounds(32 + static_cast<int>(i) * 226, 149, 216, 30);
    const auto& visible = pages[static_cast<size_t>(page)];
    for (size_t i = 0; i < visible.size(); ++i)
        knobs[static_cast<size_t>(visible[i])].setBounds(37 + static_cast<int>(i % 3) * 225, 213 + static_cast<int>(i / 3) * 122, 195, 103);
    piano.setBounds(32, 601, 936, 74);
}
void OracleEditor::timerCallback() { peak = std::max(processor.outputPeak.load(), peak * 0.92f); repaint(); }
void OracleEditor::paint(juce::Graphics& g) {
    g.fillAll(background); g.setColour(wood); g.fillRect(0, 0, 12, getHeight()); g.fillRect(getWidth() - 12, 0, 12, getHeight());
    text(g, "TRIAD ORACLE", {31, 20, 550, 36}, 29, gold);
    text(g, "FIVE-VOICE ANALOGUE-INSPIRED POLYSYNTH", {33, 58, 570, 20}, 11, muted);
    g.setColour(panel); g.fillRoundedRectangle(24, 191, 689, 391, 12); g.fillRoundedRectangle(729, 191, 247, 391, 12);
    const auto& visible = pages[static_cast<size_t>(page)];
    for (size_t i = 0; i < visible.size(); ++i)
        text(g, oracleControls[static_cast<size_t>(visible[i])].label, {37 + static_cast<int>(i % 3) * 225, 198 + static_cast<int>(i / 3) * 122, 195, 23}, 10, muted, juce::Justification::centred);
    text(g, "VOICE ACTIVITY", {748, 212, 205, 22}, 11, muted);
    const int active = processor.voiceCount.load();
    for (int i = 0; i < 5; ++i) { g.setColour(i < active ? gold : juce::Colour(0xff41454e)); g.fillEllipse(751.f + i * 39, 251, 20, 20); }
    text(g, juce::String(active) + " / 5 voices", {748, 289, 205, 28}, 23, white);
    text(g, "OUTPUT", {748, 345, 205, 20}, 11, muted);
    const float db = 20 * std::log10(std::max(peak, 1e-9f));
    text(g, (db < -90 ? juce::String("--") : juce::String(db, 1)) + " dBFS", {748, 373, 205, 32}, 25, white);
    text(g, "SAW / PULSE / TRIANGLE", {748, 438, 205, 20}, 10, gold);
    text(g, "4-pole resonant filter", {748, 465, 205, 20}, 12, muted);
    text(g, "Velocity / sustain / pitch bend", {748, 492, 212, 20}, 11, muted);
    text(g, "Mod wheel adds vibrato", {748, 519, 205, 20}, 11, muted);
    text(g, "Click keys below to audition", {748, 549, 205, 18}, 10, gold);
    text(g, "MIDI INSTRUMENT  /  LOAD ON A MIDI TRACK  /  DOUBLE-CLICK KNOBS TO RESET", {32, 687, 770, 20}, 10, muted);
    text(g, "TRIAD AUDIO  v0.1", {807, 687, 160, 20}, 10, muted, juce::Justification::centredRight);
}
