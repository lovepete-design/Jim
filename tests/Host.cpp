#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int blockSize = 256;
constexpr double pi = 3.14159265358979323846;
using Instance = std::unique_ptr<juce::AudioPluginInstance>;

// A bare Xvfb server has no window manager to register these protocol atoms.
// Keep this connection alive so the server doesn't reset them before JUCE opens.
struct TestDisplay {
   #if JUCE_LINUX || JUCE_BSD
    juce::DynamicLibrary library;
    void* display=nullptr;
    using Close = int (*)(void*);
    Close closeDisplay=nullptr;
    TestDisplay() {
        if (!library.open("libX11.so.6")) throw std::runtime_error("X11 library unavailable");
        using Open = void* (*)(const char*);
        using Intern = unsigned long (*)(void*,const char*,int);
        auto open=reinterpret_cast<Open>(library.getFunction("XOpenDisplay"));
        auto intern=reinterpret_cast<Intern>(library.getFunction("XInternAtom"));
        closeDisplay=reinterpret_cast<Close>(library.getFunction("XCloseDisplay"));
        if (!open || !intern || !closeDisplay || !(display=open(nullptr)))
            throw std::runtime_error("An X display is required; run host tests with xvfb-run -a.");
        for (const auto* name : {"WM_PROTOCOLS","WM_TAKE_FOCUS","WM_DELETE_WINDOW",
                                 "_NET_WM_PING","WM_CHANGE_STATE","WM_STATE",
                                 "_NET_WM_WINDOW_TYPE","_NET_WM_STATE","_NET_WM_STATE_HIDDEN",
                                 "_NET_WM_STATE_FULLSCREEN","_NET_WM_MOVERESIZE","_NET_FRAME_EXTENTS",
                                 "_NET_WORKAREA","_MOTIF_WM_HINTS","_WIN_HINTS","KWM_WIN_DECORATION",
                                 "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE","_NET_WM_ALLOWED_ACTIONS",
                                 "_NET_WM_WINDOW_TYPE_COMBO","_NET_WM_WINDOW_TYPE_NORMAL",
                                 "_NET_CURRENT_DESKTOP","_NET_WM_DESKTOP","_NET_SUPPORTED"})
            intern(display,name,0);
    }
    ~TestDisplay() { if (display) closeDisplay(display); }
   #endif
};

void require(bool condition, const juce::String& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

void dispatch(int milliseconds = 10) {
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}

Instance instantiate(juce::VST3PluginFormat& format, const juce::PluginDescription& description) {
    juce::String error;
    auto result = format.createInstanceFromDescription(description, 48000, blockSize, error);
    require(result != nullptr, description.name + ": instantiate failed: " + error);
    require(result->enableAllBuses(), description.name + ": could not enable buses");
    require(result->getTotalNumOutputChannels() == 2, description.name + ": expected stereo output");
    return result;
}

void prepare(juce::AudioPluginInstance& plugin, double rate) {
    plugin.setRateAndBufferSizeDetails(rate, blockSize);
    plugin.prepareToPlay(rate, blockSize);
}

double verifyBuffer(const juce::AudioBuffer<float>& buffer, const juce::String& context) {
    double energy = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            const float x = buffer.getSample(channel, sample);
            require(std::isfinite(x) && std::abs(x) <= 10000.f, context + ": invalid or unbounded audio");
            energy += static_cast<double>(x) * x;
        }
    return energy;
}

void fillInput(juce::AudioBuffer<float>& buffer, bool synth, double rate, int block) {
    buffer.clear();
    if (synth) return;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        const float x = static_cast<float>(0.4 * std::sin(2 * pi * 440 * (block * blockSize + sample) / rate));
        buffer.setSample(0, sample, x);
        buffer.setSample(1, sample, x * .7f);
    }
}

void processSilence(juce::AudioPluginInstance& plugin, int blocks = 4) {
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block) {
        buffer.clear(); midi.clear();
        plugin.processBlock(buffer, midi);
        verifyBuffer(buffer, plugin.getName() + " silent processing");
    }
}

std::vector<float> render(juce::AudioPluginInstance& plugin, bool synth, double rate) {
    prepare(plugin, rate);
    // Let state queues and smoothed controls settle identically before excitation.
    processSilence(plugin, 64);
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> result;
    result.reserve(2 * blockSize * 20);
    double energy = 0;
    for (int block = 0; block < 20; ++block) {
        fillInput(buffer, synth, rate, block);
        midi.clear();
        if (synth && block == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 73);
        if (synth && block == 12) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 101);
        plugin.processBlock(buffer, midi);
        energy += verifyBuffer(buffer, plugin.getName() + " rendering");
        if (synth && block == 0)
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 73; ++sample)
                    require(std::abs(buffer.getSample(channel, sample)) <= 1.e-9f,
                            plugin.getName() + ": audio preceded sample-offset MIDI note");
        for (int channel = 0; channel < 2; ++channel)
            result.insert(result.end(), buffer.getReadPointer(channel), buffer.getReadPointer(channel) + blockSize);
    }
    require(energy > 1.e-7, plugin.getName() + ": render was silent");
    return result;
}

juce::AudioProcessorParameter* outputParameter(juce::AudioPluginInstance& plugin) {
    for (auto* parameter : plugin.getParameters())
        if (parameter->getName(128).equalsIgnoreCase("Output")) return parameter;
    throw std::runtime_error(plugin.getName().toStdString() + ": Output parameter missing");
}

void setParameter(juce::AudioProcessorParameter& parameter, float normalized) {
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost(normalized);
    parameter.endChangeGesture();
}

void validateParameters(juce::AudioPluginInstance& plugin) {
    require(!plugin.getParameters().isEmpty(), plugin.getName() + ": no host parameters");
    for (auto* parameter : plugin.getParameters()) {
        const float value = parameter->getValue();
        const float initial = parameter->getDefaultValue();
        require(std::isfinite(value) && value >= 0 && value <= 1,
                plugin.getName() + ": invalid normalized value for " + parameter->getName(128));
        require(std::isfinite(initial) && initial >= 0 && initial <= 1,
                plugin.getName() + ": invalid default for " + parameter->getName(128));
    }
}

void exerciseAutomation(juce::AudioPluginInstance& plugin, bool synth) {
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    int block = 0;
    for (auto* parameter : plugin.getParameters()) {
        if (!parameter->isAutomatable() || parameter->isMetaParameter()) continue;
        for (float position : {0.f, 1.f, .37f}) {
            setParameter(*parameter, position);
            fillInput(buffer, synth, 48000, block++);
            midi.clear();
            if (synth) {
                midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 3);
            }
            plugin.processBlock(buffer, midi);
            verifyBuffer(buffer, plugin.getName() + " automation " + parameter->getName(128));
            validateParameters(plugin);
        }
    }
}

void editorCheck(juce::AudioPluginInstance& plugin, const juce::File& screenshotDirectory) {
    require(plugin.hasEditor(), plugin.getName() + ": no editor advertised");
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin.createEditorIfNeeded());
    require(editor != nullptr, plugin.getName() + ": editor creation failed");
    require(editor->getWidth() > 100 && editor->getHeight() > 100, plugin.getName() + ": invalid editor size");
    editor->addToDesktop(0);
    editor->setTopLeftPosition(20, 20);
    editor->setVisible(true);
    editor->toFront(false);
    dispatch(100);

    if (screenshotDirectory != juce::File()) {
        require(screenshotDirectory.createDirectory().wasOk(), "Could not create screenshot directory");
        const auto file = screenshotDirectory.getChildFile(plugin.getName().replaceCharacter(' ', '-') + ".png");
        juce::StringArray command;
       #if JUCE_LINUX || JUCE_BSD
        // The editor contains a native child window. A JUCE component snapshot
        // would capture the black host wrapper, so use native X11 capture.
        const auto native = reinterpret_cast<std::uintptr_t>(editor->getWindowHandle());
        command.addArray({"import", "-window", "0x" + juce::String::toHexString(static_cast<juce::int64>(native)), file.getFullPathName()});
       #elif JUCE_MAC
        const auto bounds = editor->getScreenBounds();
        command.addArray({"screencapture", "-x", "-R" + juce::String(bounds.getX()) + "," + juce::String(bounds.getY()) + "," + juce::String(bounds.getWidth()) + "," + juce::String(bounds.getHeight()), file.getFullPathName()});
       #endif
        if (!command.isEmpty()) {
            juce::ChildProcess capture;
            if (capture.start(command) && capture.waitForProcessToFinish(10000) && capture.getExitCode() == 0 && file.existsAsFile())
                std::cout << "  Screenshot: " << file.getFullPathName() << '\n';
            else
                std::cout << "  Screenshot unavailable (native capture utility or display permission missing).\n";
        }
    }
    editor->setVisible(false);
    // Destroy the plugin view before its native parent window.
    editor.reset();
    dispatch();
}

void validate(juce::VST3PluginFormat& format, const juce::String& path, bool synth,
              const juce::File& screenshotDirectory) {
    require(juce::File(path).exists(), "Plugin bundle missing: " + path);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, path);
    require(descriptions.size() == 1, "Expected one VST3 class in " + path + ", found " + juce::String(descriptions.size()));
    const auto description = *descriptions[0];
    std::cout << "Validating " << description.name << "..." << std::endl;
    auto plugin = instantiate(format, description);
    require(plugin->acceptsMidi() == synth, description.name + ": unexpected MIDI capability");
    require(plugin->getTotalNumInputChannels() == (synth ? 0 : 2), description.name + ": unexpected input layout");
    validateParameters(*plugin);
    const auto defaultRender = render(*plugin, synth, 48000);
    juce::ignoreUnused(defaultRender);
    editorCheck(*plugin, screenshotDirectory);

    auto* output = outputParameter(*plugin);
    setParameter(*output, .43f);
    processSilence(*plugin, 64);
    dispatch();
    const float storedOutput = output->getValue();
    juce::MemoryBlock state;
    plugin->getStateInformation(state);
    require(state.getSize() > 16, description.name + ": empty state");
    setParameter(*output, .82f);
    processSilence(*plugin);
    require(std::abs(output->getValue() - storedOutput) > .1f, description.name + ": parameter did not change");
    // Restoring while stopped must still work after many unprocessed patch changes.
    for (int change=0; change<40; ++change)
        plugin->setCurrentProgram(change % juce::jmax(1,plugin->getNumPrograms()));
    plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    dispatch();
    processSilence(*plugin);
    require(std::abs(output->getValue() - storedOutput) < 1.e-5f, description.name + ": state did not restore Output");

    auto second = instantiate(format, description);
    second->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    dispatch();
    require(std::abs(outputParameter(*second)->getValue() - storedOutput) < 1.e-5f,
            description.name + ": recreated instance did not recall Output");
    // Both instances coexist. Their saved patches and reset renders must agree.
    plugin->releaseResources();
    const auto firstRender = render(*plugin, synth, 48000);
    const auto secondRender = render(*second, synth, 48000);
    require(firstRender.size() == secondRender.size(), description.name + ": inconsistent render lengths");
    double maxDifference = 0;
    for (std::size_t i = 0; i < firstRender.size(); ++i)
        maxDifference = std::max(maxDifference, static_cast<double>(std::abs(firstRender[i] - secondRender[i])));
    require(maxDifference < 2.e-5, description.name + ": recreated state render differs by " + juce::String(maxDifference, 8));

    exerciseAutomation(*plugin, synth);
    plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    dispatch();
    for (double rate : {44100., 96000.}) {
        plugin->releaseResources();
        render(*plugin, synth, rate);
    }
    // Check that processing one live instance did not corrupt another.
    second->releaseResources();
    const auto repeatedSecond = render(*second, synth, 48000);
    double repeatDifference = 0;
    for (std::size_t i = 0; i < repeatedSecond.size(); ++i)
        repeatDifference = std::max(repeatDifference, static_cast<double>(std::abs(repeatedSecond[i] - secondRender[i])));
    require(repeatDifference < 2.e-5, description.name + ": instance isolation/reset failed");
    second->releaseResources(); plugin->releaseResources();
    second.reset(); plugin.reset();
    dispatch();
    std::cout << "  PASS: stereo audio, MIDI timing, editor, state recall, automation, instance isolation, 44.1/48/96 kHz.\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<juce::String> paths;
        juce::File screenshots;
        for (int index = 1; index < argc; ++index) {
            const juce::String argument(argv[index]);
            if (argument == "--screenshots") {
                require(index + 1 < argc, "--screenshots requires a directory");
                screenshots = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++index]);
            } else paths.push_back(argument);
        }
        require(paths.size() == 3, "Usage: triad_host Hex-MkI.vst3 Stress.vst3 Lattice.vst3 [--screenshots DIR]");
        TestDisplay display;
        juce::ScopedJuceInitialiser_GUI initialiser;
        juce::VST3PluginFormat format;
        for (std::size_t index = 0; index < paths.size(); ++index)
            validate(format, paths[index], index != 1, screenshots);
        std::cout << "All three VST3 integration checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VST3 validation failed: " << error.what() << '\n';
        return 1;
    }
}
