#include "PitchEngine.h"
#include "ManualClip.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>

void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
double estimate(const std::vector<float>& audio, double rate) {
    triad::Detector detector; detector.prepare(rate);
    for (float sample : audio) detector.push(sample);
    return detector.frequency;
}
std::vector<float> tone(double hz, double rate, double seconds) {
    std::vector<float> result(static_cast<size_t>(rate * seconds));
    for (size_t i = 0; i < result.size(); ++i) result[i] = static_cast<float>(0.3 * std::sin(2 * triad::pi * hz * i / rate));
    return result;
}
int main() {
    require(triad::nearestNote(61.2, 0, triad::Scale::Major) == 62, "C major snaps C# toward D");
    require(triad::nearestNote(68.7, 9, triad::Scale::Minor) == 69, "A minor accepts A");
    for (double hz : {70., 110., 220., 440., 880.}) {
        const auto measured = estimate(tone(hz, 12000, 0.5), 12000);
        require(std::abs(1200 * std::log2(measured / hz)) < 6, "YIN frequency accuracy");
    }
    require(estimate(std::vector<float>(12000, 0), 12000) == 0, "Silence is unvoiced");
    std::mt19937 random(42); std::uniform_real_distribution<float> noise(-0.2f, 0.2f);
    std::vector<float> unvoiced(12000); for (auto& v : unvoiced) v = noise(random);
    require(estimate(unvoiced, 12000) == 0, "Noise is unvoiced");
    for (double rate : {44100., 48000., 96000.}) {
        triad::Engine engine; engine.prepare(rate);
        auto left = tone(430, rate, 1.5), right = left;
        triad::Settings settings; settings.retuneMs = 0; settings.humanizeCents = 0;
        for (size_t i = 0; i < left.size(); i += 127) {
            float* channels[] {left.data() + i, right.data() + i};
            engine.process(channels, 2, static_cast<int>(std::min<size_t>(127, left.size() - i)), settings);
        }
        require(std::abs(engine.inputMidi - (69 + 12 * std::log2(430. / 440))) < 0.05, "Live input note tracking");
        require(left == right, "Stereo phase remains linked");
        std::vector<float> decimated;
        const int stride = static_cast<int>(rate / 12000);
        for (size_t i = left.size() / 2; i < left.size(); i += static_cast<size_t>(stride)) {
            require(std::isfinite(left[i]) && std::abs(left[i]) < 0.5f, "Finite bounded live output");
            decimated.push_back(left[i]);
        }
        const double corrected = estimate(decimated, rate / stride);
        require(std::abs(1200 * std::log2(corrected / 440)) < 15, "Live output tunes 430 Hz toward A440");
        // Dry path must match its advertised latency exactly after mix settles.
        engine.prepare(rate); settings.mix = 0;
        auto dry = tone(220, rate, 0.5), original = dry;
        float* mono[] {dry.data()}; engine.process(mono, 1, static_cast<int>(dry.size()), settings);
        for (size_t i = static_cast<size_t>(rate * 0.25); i < dry.size(); ++i)
            require(std::abs(dry[i] - original[i - static_cast<size_t>(engine.getLatency())]) < 1e-5, "Dry path latency compensation");
    }
    triad::ManualClip clip; clip.prepare(48000);
    auto captured = tone(220, 48000, 1.5);
    require(clip.arm(), "Transfer can arm");
    for (size_t i = 0; i < captured.size(); i += 256) {
        float* channels[] {captured.data() + i};
        clip.capture(channels, 1, static_cast<int>(std::min<size_t>(256, captured.size() - i)), 10 + i / 48000.0, true);
    }
    clip.stop();
    float dummy = 0; float* dummyChannels[] {&dummy}; clip.capture(dummyChannels, 1, 1, 11.5, false);
    clip.service();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!clip.ready() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    require(clip.ready() && clip.countNotes() > 0, "Transferred audio produces editable notes");
    require(std::abs(clip.note(0).original - 57) < 0.1f, "Transferred A3 detected correctly");
    std::vector<float> output(captured.size(), 0); float* outChannels[] {output.data()};
    clip.playback(outChannels, 1, static_cast<int>(output.size()), 10);
    require(output == captured, "Unedited clip playback has exactly zero added delay");
    for (int i = 0; i < clip.countNotes(); ++i) clip.transpose(i, 12);
    clip.playback(outChannels, 1, static_cast<int>(output.size()), 10);
    std::vector<float> edited;
    for (size_t i = 24000; i < 48000; i += 4) edited.push_back(output[i]);
    require(std::abs(1200 * std::log2(estimate(edited, 12000) / 440)) < 20, "Dragging A3 up an octave produces A4");
    // Random-access host playback must reproduce the same samples after seeking.
    std::vector<float> seek(512, 0); float* seekChannels[] {seek.data()};
    clip.playback(seekChannels, 1, 512, 10.5);
    for (size_t i = 0; i < seek.size(); ++i) require(std::abs(seek[i] - output[24000 + i]) < 1e-6, "Edits stay aligned after DAW seek");
    const auto saved = clip.snapshot();
    triad::ManualClip recalled; recalled.prepare(48000); recalled.restore(saved);
    std::fill(seek.begin(), seek.end(), 0);
    recalled.playback(seekChannels, 1, 512, 10.5);
    for (size_t i = 0; i < seek.size(); ++i) require(std::abs(seek[i] - output[24000 + i]) < 1e-6, "Capture and note edits survive state recall");
    recalled.prepare(96000);
    require(recalled.ready() && std::abs(recalled.duration() - 1.5) < 1e-6, "Capture survives host sample rate change");
    require(recalled.note(0).transpose.load() == 12, "Note edit survives host sample rate change");
    recalled.reset(0);
    require(!recalled.note(0).tuned.load(), "Double-click reset restores original pitch");
    std::cout << "PASS: scale mapping, YIN, noise rejection, live correction at 3 sample rates, stereo, latency, transfer, note edits, host seek, state recall, and sample rate changes\n";
}
