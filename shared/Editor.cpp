#include "Editor.h"
#include "Plugin.h"

namespace
{
constexpr auto background = 0xff111519;
constexpr auto panel = 0xff1a2026;
constexpr auto ink = 0xffedf0ed;
constexpr auto muted = 0xff8e9a9f;

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

class StudioLook final : public juce::LookAndFeel_V4
{
public:
    explicit StudioLook (juce::Colour colour) : accent (colour)
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (ink));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff242d34));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff35414a));
        setColour (juce::ComboBox::textColourId, juce::Colour (ink));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (panel));
        setColour (juce::PopupMenu::textColourId, juce::Colour (ink));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.darker (0.65f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff273139));
        setColour (juce::TextButton::textColourOffId, juce::Colour (ink));
        setColour (juce::ToggleButton::textColourId, juce::Colour (ink));
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (muted));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13.0f, true); }
    juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                          float position, float start, float end, juce::Slider&) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                        .reduced (9.0f);
        const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto centre = area.getCentre();
        const auto angle = start + position * (end - start);
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                             0.0f, start, end, true);
        g.setColour (juce::Colour (0xff354049));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        juce::Path active;
        active.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                              0.0f, start, angle, true);
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        auto knob = juce::Rectangle<float> (radius * 1.40f, radius * 1.40f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b4750), knob.getTopLeft(),
                                               juce::Colour (0xff202930), knob.getBottomRight(), false));
        g.fillEllipse (knob);
        g.setColour (juce::Colour (0xff53606a));
        g.drawEllipse (knob, 1.0f);
        juce::Path tick;
        tick.addRoundedRectangle (-1.5f, -radius * 0.57f, 3.0f, radius * 0.25f, 1.0f);
        g.setColour (juce::Colour (ink));
        g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

private:
    juce::Colour accent;
};

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id,
          const juce::String& name, const juce::String& suffix, const juce::String& tooltip)
    {
        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (muted));
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 108, 22);
        slider.setTextValueSuffix (suffix);
        slider.setTooltip (tooltip);
        slider.setName (name);
        slider.setTitle (name);
        addAndMakeVisible (label);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (state, id, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (23));
        slider.setBounds (r.reduced (4, 0));
    }

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Meter final : public juce::Component
{
public:
    Meter (TriadProcessor& p) : processor (p) {}

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff101519));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (muted));
        g.setFont (font (11.0f, true));
       #if TRIAD_KIND == 1
        g.drawText ("GAIN REDUCTION", r.withHeight (28.0f), juce::Justification::centred);
        const float reading = juce::jlimit (0.0f, 60.0f, processor.meter());
        const float normal = reading / 30.0f;
        const juce::String value = juce::String (reading, 1) + " dB";
       #else
        g.drawText ("OUTPUT", r.withHeight (28.0f), juce::Justification::centred);
        const float peak = juce::jmax (0.0f, processor.meter());
        const float reading = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float normal = (reading + 60.0f) / 60.0f;
        const juce::String value = reading <= -59.9f ? "-inf dB" : juce::String (reading, 1) + " dB";
       #endif
        auto bars = r.reduced (18.0f, 0.0f).withTrimmedTop (38.0f).withTrimmedBottom (40.0f);
        constexpr int segments = 20;
        const float step = bars.getHeight() / (float) segments;
        for (int i = 0; i < segments; ++i)
        {
            const bool lit = normal > (float) i / (float) segments;
            auto colour = processor.accent();
           #if TRIAD_KIND != 1
            if (i >= 18) colour = juce::Colour (0xffec826d);
           #endif
            g.setColour (lit ? colour : juce::Colour (0xff29333b));
            g.fillRoundedRectangle (bars.getX(), bars.getBottom() - (float) (i + 1) * step,
                                    bars.getWidth(), juce::jmax (1.0f, step - 3.0f), 1.5f);
        }
        g.setFont (font (14.0f));
        g.setColour (juce::Colour (ink));
        g.drawText (value, r.withTrimmedTop (r.getHeight() - 34.0f), juce::Justification::centred);
    }
private:
    TriadProcessor& processor;
};
}

struct TriadEditor::Content
{
    Content (TriadEditor& e, TriadProcessor& p)
        : editor (e), processor (p), look (p.accent()), meter (p),
          keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), tooltips (&e, 600)
    {
        editor.setLookAndFeel (&look);
        presetLabel.setText ("PRESET", juce::dontSendNotification);
        presetLabel.setColour (juce::Label::textColourId, juce::Colour (muted));
        presetLabel.setJustificationType (juce::Justification::centredLeft);
        presets.addItemList (processor.presetNames(), 1);
        presets.setTextWhenNothingSelected ("Choose a starting point…");
        presets.setTooltip ("Choose a starting point. Presets replace the current settings.");
        presets.setTitle ("Preset");
        presets.onChange = [this] { processor.selectPreset (presets.getSelectedItemIndex()); };
        editor.addAndMakeVisible (presetLabel);
        editor.addAndMakeVisible (presets);
        editor.addAndMakeVisible (meter);

       #if TRIAD_KIND == 0
        addKnob ("algorithm", "Algorithm", "", "Choose one of the DX7's 32 operator routings.");
        addKnob ("feedback", "Feedback", "", "Feedback around the algorithm's feedback operator.");
        addKnob ("tune", "Fine tune", " ct", "Global pitch offset in cents.");
        addKnob ("output", "Output", " dB", "Output level after the FM voice engine.");
        importButton.setButtonText ("Import DX7 SysEx");
        importButton.setTooltip ("Load a DX7 single-voice or 32-voice bank .syx file.");
        importButton.onClick = [this] { importSysex(); };
        editor.addAndMakeVisible (importButton);
       #elif TRIAD_KIND == 1
        addKnob ("input", "Input", " dB", "Drive the fixed -18 dBFS compression threshold.");
        addKnob ("attack", "Attack", " ms", "Time for gain reduction to respond to transients.");
        addKnob ("release", "Release", " ms", "Recovery time after the detector falls.");
        addKnob ("output", "Output", " dB", "Makeup gain after compression and harmonic colour.");
        addKnob ("mix", "Mix", " %", "Blend the dry input with the processed signal.");
        ratio.addItemList ({ "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "Limit" }, 1);
        distortion.addItemList ({ "Clean", "Even harmonics", "Odd harmonics" }, 1);
        ratio.setTitle ("Ratio");
        distortion.setTitle ("Harmonic colour");
        ratio.setTooltip ("Compression ratio. 10:1 uses a program-dependent recovery character.");
        distortion.setTooltip ("Independent harmonic models, inspired by analog colour.");
        ratioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            (processor.parameters, "ratio", ratio);
        distortionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            (processor.parameters, "distortion", distortion);
        highpass.setButtonText ("Detector HPF");
        emphasis.setButtonText ("HF emphasis");
        highpass.setTooltip ("Reduce bass sensitivity in the sidechain detector.");
        emphasis.setTooltip ("Increase the detector's sensitivity to high frequencies.");
        highpassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
            (processor.parameters, "highpass", highpass);
        emphasisAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
            (processor.parameters, "emphasis", emphasis);
        for (auto* c : std::initializer_list<juce::Component*> { &ratio, &distortion, &highpass, &emphasis })
            editor.addAndMakeVisible (c);
       #else
        addKnob ("diffusion", "Diffusion", "", "Move energy between neighbouring spectral modes.");
        addKnob ("tension", "Tension", "", "Couple the phases of neighbouring oscillators.");
        addKnob ("memory", "Memory", "", "Control how long the spectral energy field retains its shape.");
        addKnob ("brightness", "Brightness", "", "Shift excitation toward the upper spectral modes.");
        addKnob ("attack", "Attack", " ms", "Amplitude onset time for each voice.");
        addKnob ("release", "Release", " ms", "Amplitude decay after note-off.");
        addKnob ("output", "Output", " dB", "Final stereo output level.");
       #endif
       #if TRIAD_KIND != 1
        keyboard.setAvailableRange (24, 108);
        keyboard.setLowestVisibleKey (48);
        keyboard.setKeyWidth (22.0f);
        keyboard.setVelocity (0.8f, true);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdce1dc));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff171c20));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, processor.accent().withAlpha (0.65f));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, processor.accent().withAlpha (0.2f));
        editor.addAndMakeVisible (keyboard);
       #endif
    }

    void addKnob (const juce::String& id, const juce::String& name,
                  const juce::String& suffix, const juce::String& help)
    {
        auto knob = std::make_unique<Knob> (processor.parameters, id, name, suffix, help);
        editor.addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    void importSysex()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import a DX7 voice or bank", juce::File {}, "*.syx");
        const juce::Component::SafePointer<TriadEditor> safe (&editor);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                const auto file = fc.getResult();
                if (! file.existsAsFile()) return;
                const auto result = safe->content->processor.loadSysexFile (file);
                if (result.failed())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                           "Could not import voice", result.getErrorMessage());
                else
                    safe->content->presets.setText (file.getFileNameWithoutExtension(), juce::dontSendNotification);
            });
    }

    TriadEditor& editor;
    TriadProcessor& processor;
    StudioLook look;
    Meter meter;
    juce::Label presetLabel;
    juce::ComboBox presets, ratio, distortion;
    juce::TextButton importButton;
    juce::ToggleButton highpass, emphasis;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ratioAttachment, distortionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highpassAttachment, emphasisAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
};

TriadEditor::TriadEditor (TriadProcessor& p) : juce::AudioProcessorEditor (p)
{
    content = std::make_unique<Content> (*this, p);
   #if TRIAD_KIND == 2
    setSize (960, 700);
   #else
    setSize (960, 600);
   #endif
    startTimerHz (30);
}

TriadEditor::~TriadEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TriadEditor::timerCallback()
{
    content->meter.repaint();
}

void TriadEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (background));
    const auto& p = content->processor;
    g.setColour (p.accent());
    g.fillRect (0, 0, getWidth(), 3);
    g.setFont (font (11.0f, true));
    g.drawText ("T R I A D   /   A U D I O", 30, 21, 400, 18, juce::Justification::centredLeft);
    g.setColour (juce::Colour (ink));
    g.setFont (font (34.0f, true));
    g.drawText (p.title(), 28, 43, 530, 45, juce::Justification::centredLeft);
    g.setColour (juce::Colour (muted));
    g.setFont (font (13.0f));
    g.drawText (p.subtitle(), 30, 92, 660, 22, juce::Justification::centredLeft);
    g.setColour (juce::Colour (panel));
   #if TRIAD_KIND == 2
    g.fillRoundedRectangle (20.0f, 136.0f, 920.0f, 408.0f, 10.0f);
   #else
    g.fillRoundedRectangle (20.0f, 136.0f, 920.0f, 316.0f, 10.0f);
   #endif
    g.setColour (juce::Colour (muted));
    g.setFont (font (11.0f, true));
   #if TRIAD_KIND == 0
    g.drawText ("VOICE ENGINE", 42, 151, 400, 20, juce::Justification::centredLeft);
    g.setColour (p.accent());
    for (int i = 0; i < 6; ++i)
    {
        const float x = 79.0f + (float) i * 102.0f;
        g.fillRoundedRectangle (x, 362.0f, 66.0f, 36.0f, 4.0f);
        g.setColour (juce::Colour (background));
        g.setFont (font (13.0f, true));
        g.drawText ("OP " + juce::String (6 - i), (int) x, 362, 66, 36, juce::Justification::centred);
        g.setColour (p.accent());
    }
    g.setColour (juce::Colour (muted));
    g.setFont (font (12.0f));
    g.drawText ("Six-operator FM  /  DX7 voice envelopes  /  Original SysEx voices", 42, 412, 670, 23,
                juce::Justification::centredLeft);
    g.drawText ("Play with MIDI or the keyboard below. Sustain: CC64. Pitch bend: ±2 semitones.",
                30, 468, 890, 25, juce::Justification::centredLeft);
   #elif TRIAD_KIND == 1
    g.drawText ("DYNAMICS & COLOUR", 42, 151, 400, 20, juce::Justification::centredLeft);
    g.drawText ("RATIO", 46, 356, 120, 20, juce::Justification::centredLeft);
    g.drawText ("HARMONICS", 226, 356, 180, 20, juce::Justification::centredLeft);
    g.setFont (font (13.0f));
    g.drawText ("Drive the input. Shape the transient. Bring back the room.",
                30, 474, 860, 25, juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawText ("Stereo-linked detection · Fixed −18 dBFS threshold · Parallel mix · Independent analog-inspired model",
                30, 506, 900, 24, juce::Justification::centredLeft);
   #else
    g.drawText ("SPECTRAL FIELD", 42, 151, 400, 20, juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawText ("Energy travels between spectral modes while their phases pull on one another.",
                30, 555, 890, 24, juce::Justification::centredLeft);
    g.drawText ("Diffusion shapes motion. Tension adds interaction. Memory lets the field persist.",
                30, 578, 890, 24, juce::Justification::centredLeft);
   #endif
    g.setColour (juce::Colour (0xff46545e));
    g.setFont (font (10.0f));
    g.drawText ("TRIAD AUDIO  /  VST3", 30, getHeight() - 25, 650, 17, juce::Justification::centredLeft);
    g.drawText ("DOUBLE-CLICK A VALUE TO TYPE", getWidth() - 290, getHeight() - 25, 260, 17,
                juce::Justification::centredRight);
}

void TriadEditor::resized()
{
    if (content == nullptr) return;
    content->presetLabel.setBounds (690, 34, 220, 20);
    content->presets.setBounds (690, 58, 240, 33);
   #if TRIAD_KIND == 0
    for (int i = 0; i < 4; ++i)
        content->knobs[(size_t) i]->setBounds (45 + i * 167, 187, 144, 146);
    content->meter.setBounds (766, 184, 142, 238);
    content->importButton.setBounds (690, 98, 240, 28);
    content->keyboard.setBounds (30, 506, 900, 62);
   #elif TRIAD_KIND == 1
    for (int i = 0; i < 5; ++i)
        content->knobs[(size_t) i]->setBounds (35 + i * 145, 184, 136, 155);
    content->meter.setBounds (790, 184, 118, 238);
    content->ratio.setBounds (44, 381, 150, 33);
    content->distortion.setBounds (224, 381, 200, 33);
    content->highpass.setBounds (465, 375, 145, 42);
    content->emphasis.setBounds (618, 375, 150, 42);
   #else
    for (int i = 0; i < 4; ++i)
        content->knobs[(size_t) i]->setBounds (48 + i * 176, 181, 150, 152);
    for (int i = 4; i < 7; ++i)
        content->knobs[(size_t) i]->setBounds (136 + (i - 4) * 176, 355, 150, 152);
    content->meter.setBounds (800, 185, 108, 322);
    content->keyboard.setBounds (30, 610, 900, 58);
   #endif
}
