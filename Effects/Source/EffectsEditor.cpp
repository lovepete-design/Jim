#include "EffectsEditor.h"
namespace {
const juce::Colour background(0xff0f131a), panel(0xff191f2a), muted(0xff97a2b4), white(0xffedf2fa);
#if TRIAD_MASTERING
const juce::Colour accent(0xffffc37b);
#else
const juce::Colour accent(0xff7be5cd);
#endif
void label(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> area, float size, juce::Colour colour,
           juce::Justification align = juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawText(value, area, align);
}
juce::String reading(float value, bool positive = false) {
    if (!std::isfinite(value) || value < -90) return "--";
    return juce::String(positive ? std::max(0.f, value) : value, 1);
}
}
EffectsLook::EffectsLook() {
    setColour(juce::ComboBox::backgroundColourId, panel); setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff354052));
    setColour(juce::ComboBox::textColourId, white); setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff354052));
    setColour(juce::Slider::textBoxTextColourId, white); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, muted); setColour(juce::ToggleButton::tickColourId, accent);
}
void EffectsLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float value,
                                   float start, float end, juce::Slider&) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(10);
    const auto centre = bounds.getCentre(); const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    juce::Path track, fill;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
    fill.addCentredArc(centre.x, centre.y, radius, radius, 0, start, start + value * (end - start), true);
    g.setColour(juce::Colour(0xff344052)); g.strokePath(track, juce::PathStrokeType(3.5f));
    g.setColour(accent); g.strokePath(fill, juce::PathStrokeType(3.5f));
    g.setColour(juce::Colour(0xff252e3d)); g.fillEllipse(centre.x - radius + 7, centre.y - radius + 7, 2 * (radius - 7), 2 * (radius - 7));
    const float angle = start + value * (end - start);
    g.setColour(white); g.drawLine(centre.x + std::sin(angle) * (radius - 19), centre.y - std::cos(angle) * (radius - 19),
        centre.x + std::sin(angle) * (radius - 10), centre.y - std::cos(angle) * (radius - 10), 2);
}
EffectsEditor::EffectsEditor(EffectsProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&look);
#if TRIAD_MASTERING
    setSize(1080, 800);
    presets.addItemList({"Clean starting point", "Gentle glue", "Warm & wide", "Louder preview"}, 1);
#else
    setSize(900, 620);
    presets.addItemList({"Gentle vocal", "Strong sibilance", "Bright vocal"}, 1);
#endif
    presets.setTextWhenNothingSelected("Starting points"); presets.onChange = [this] { applyPreset(presets.getSelectedId()); };
    addAndMakeVisible(presets);
    for (size_t i = 0; i < knobs.size(); ++i) {
        const auto& spec = triadfx::parameters[i]; auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 112, 24); knob.setTextValueSuffix(spec.suffix);
        knob.setTooltip(spec.help); knob.setName(spec.label);
        knob.setDoubleClickReturnValue(true, spec.initial); addAndMakeVisible(knob);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, spec.id, knob);
    }
    for (size_t i = 0; i < buttons.size(); ++i) {
        buttons[i].setButtonText(triadfx::flagLabels[i]); addAndMakeVisible(buttons[i]);
        buttonAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, triadfx::flagIds[i], buttons[i]);
    }
    startTimerHz(30);
}
EffectsEditor::~EffectsEditor() { stopTimer(); setLookAndFeel(nullptr); }
void EffectsEditor::resized() {
    presets.setBounds(getWidth() - 247, 26, 215, 34);
#if TRIAD_MASTERING
    for (size_t i = 0; i < buttons.size(); ++i) buttons[i].setBounds(35 + static_cast<int>(i) * 175, 86, 160, 28);
    for (size_t i = 0; i < knobs.size(); ++i) {
        const int row = static_cast<int>(i) / 4, col = static_cast<int>(i) % 4;
        knobs[i].setBounds(35 + col * 188, 154 + row * 143, 166, 117);
    }
#else
    buttons[0].setBounds(34, 83, 145, 28); buttons[1].setBounds(198, 83, 240, 28); buttons[2].setBounds(490, 83, 120, 28);
    for (size_t i = 0; i < knobs.size(); ++i) knobs[i].setBounds(28 + static_cast<int>(i % 4) * 164, 265 + static_cast<int>(i / 4) * 145, 156, 119);
#endif
}
void EffectsEditor::timerCallback() {
    for (size_t i = 0; i < displayed.size(); ++i) {
        const float value = processor.meters[i].load(std::memory_order_relaxed);
        if (!std::isfinite(value)) continue;
        displayed[i] = std::max(value, displayed[i] - (i == 3 || i == 4 ? 0.35f : 1.0f));
    }
    reductionHistory[cursor] = std::max(0.f, displayed[3]); cursor = (cursor + 1) % reductionHistory.size(); repaint();
}
void EffectsEditor::applyPreset(int id) {
    if (id < 1) return;
    for (const auto& p : triadfx::parameters) {
        auto* parameter = processor.state.getParameter(p.id);
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(p.initial)); parameter->endChangeGesture();
    }
    auto set = [&](const char* key, float value) {
        if (auto* parameter = processor.state.getParameter(key)) {
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
        }
    };
    for (size_t i = 0; i < triadfx::flagIds.size(); ++i) set(triadfx::flagIds[i], triadfx::flagDefaults[i] ? 1.f : 0.f);
#if TRIAD_MASTERING
    if (id == 2) { set("threshold", -22); set("ratio", 1.8f); set("attack", 40); set("makeup", 1); }
    if (id == 3) { set("low", 1); set("high", 0.5f); set("saturation", 10); set("width", 115); set("threshold", -20); }
    if (id == 4) { set("threshold", -20); set("ratio", 2); set("loudness", 4); set("ceiling", -1); }
#else
    if (id == 1) { set("threshold", -22); set("ratio", 3); set("range", 6); set("attack", 1); set("release", 100); }
    if (id == 2) { set("threshold", -28); set("ratio", 6); set("range", 12); set("attack", 0.3f); set("release", 70); }
    if (id == 3) { set("frequency", 8500); set("threshold", -25); set("ratio", 4); set("range", 8); }
#endif
}
void EffectsEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    label(g, triadfx::productName, {30, 18, 530, 36}, 28, accent);
#if TRIAD_MASTERING
    label(g, "TONE  /  GLUE  /  COLOUR  /  STEREO  /  LIMIT", {32, 55, 650, 20}, 11, muted);
    g.setColour(panel); g.fillRoundedRectangle(24, 125, 766, 595, 12);
    const char* sections[] {"INPUT & LOW END", "TONE & DYNAMICS", "ENVELOPE & COLOUR", "STEREO & OUTPUT"};
    for (int row = 0; row < 4; ++row) {
        label(g, sections[row], {40, 130 + row * 143, 700, 18}, 10, accent.withAlpha(0.8f));
        for (int col = 0; col < 4; ++col) {
            const size_t i = static_cast<size_t>(row * 4 + col);
            label(g, triadfx::parameters[i].label, {35 + col * 188, 148 + row * 143, 166, 20}, 10, muted, juce::Justification::centred);
        }
    }
    const int meterX = 812;
#else
    label(g, "PRECISE SIBILANCE CONTROL", {32, 55, 540, 20}, 11, muted);
    g.setColour(panel); g.fillRoundedRectangle(24, 125, 650, 102, 12); g.fillRoundedRectangle(24, 242, 650, 319, 12);
    label(g, "GAIN REDUCTION HISTORY", {40, 133, 500, 18}, 10, muted);
    juce::Path path;
    for (size_t i = 0; i < reductionHistory.size(); ++i) {
        const float x = 42 + static_cast<float>(i) * 610 / static_cast<float>(reductionHistory.size() - 1);
        const float y = 163 + std::min(18.f, reductionHistory[(cursor + i) % reductionHistory.size()]) / 18 * 45;
        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    g.setColour(accent); g.strokePath(path, juce::PathStrokeType(2));
    for (size_t i = 0; i < knobs.size(); ++i)
        label(g, triadfx::parameters[i].label, {28 + static_cast<int>(i % 4) * 164, 251 + static_cast<int>(i / 4) * 145, 156, 24}, 10, muted, juce::Justification::centred);
    if (processor.state.getRawParameterValue("listen")->load() > 0.5f)
        label(g, "MONITORING DETECTOR BAND", {35, 565, 600, 20}, 11, accent);
    const int meterX = 698;
#endif
    g.setColour(panel); g.fillRoundedRectangle(static_cast<float>(meterX), 125, static_cast<float>(getWidth() - meterX - 24), static_cast<float>(getHeight() - 181), 12);
    const char* meterNames[] {"INPUT PEAK", "OUTPUT PEAK", "OUTPUT RMS", "COMPRESSION", "LIMITING", "DETECTOR"};
    const int number = TRIAD_MASTERING ? 5 : 4;
    for (int i = 0; i < number; ++i) {
        const int source = !TRIAD_MASTERING && i == 3 ? 5 : i;
        const int y = 143 + i * (TRIAD_MASTERING ? 104 : 88);
        label(g, meterNames[source], {meterX + 18, y, 200, 18}, 10, muted);
        label(g, reading(displayed[static_cast<size_t>(source)], source == 3 || source == 4) + " dB", {meterX + 18, y + 20, 200, 33}, 25, white);
        const float normalized = source == 3 || source == 4 ? std::clamp(displayed[static_cast<size_t>(source)] / 18, 0.f, 1.f)
            : std::clamp((displayed[static_cast<size_t>(source)] + 60) / 60, 0.f, 1.f);
        g.setColour(juce::Colour(0xff344052)); g.fillRoundedRectangle(static_cast<float>(meterX + 18), static_cast<float>(y + 58), static_cast<float>(getWidth() - meterX - 60), 4, 2);
        g.setColour(source == 1 && displayed[1] >= 0 ? juce::Colours::orangered : accent);
        g.fillRoundedRectangle(static_cast<float>(meterX + 18), static_cast<float>(y + 58), (getWidth() - meterX - 60) * normalized, 4, 2);
    }
#if !TRIAD_MASTERING
    label(g, "REDUCTION", {meterX + 18, 494, 170, 18}, 10, accent);
    label(g, reading(displayed[3], true) + " dB", {meterX + 18, 514, 180, 33}, 25, accent);
#endif
    g.setColour(juce::Colour(0xff303b4b)); g.drawHorizontalLine(getHeight() - 41, 30, static_cast<float>(getWidth() - 30));
    label(g, TRIAD_MASTERING ? "2 ms LOOK-AHEAD  /  SAMPLE-PEAK LIMITER  /  RMS METERING" : "ZERO ADDED LATENCY  /  STEREO LINKED DETECTOR", {30, getHeight() - 33, getWidth() - 245, 20}, 10, muted);
    label(g, "TRIAD AUDIO   v0.1", {getWidth() - 230, getHeight() - 33, 200, 20}, 10, muted, juce::Justification::centredRight);
}
