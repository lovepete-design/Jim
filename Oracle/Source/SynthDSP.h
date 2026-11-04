#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace oracle {
constexpr double pi = 3.14159265358979323846;
struct Settings {
    double mix = 0.5, detune = 7, coarse = 0, pulse = 0.5, noise = 0, cutoff = 1800, resonance = 0.2, filterAmount = 2;
    double ampAttack = 10, ampDecay = 400, ampSustain = 0.7, ampRelease = 500;
    double filterAttack = 10, filterDecay = 600, filterSustain = 0.25, filterRelease = 400;
    double lfoRate = 5, vibrato = 0, filterLfo = 0, polyB = 0, polyEnv = 0, chorus = 0.1, spread = 0.35, drift = 0.1, volume = -6;
    int waveA = 0, waveB = 0;
    bool sync = false, keytrack = true;
};
struct Envelope {
    enum Stage { Off, Attack, Decay, Sustain, Release } stage = Off;
    double value = 0, releaseStep = 0;
    void start() { value = 0; stage = Attack; }
    void stop(double ms, double rate) { releaseStep = value / std::max(1.0, ms * rate * 0.001); stage = Release; }
    double next(double a, double d, double s, double rate) {
        if (stage == Attack) { value += 1 / std::max(1.0, a * rate * 0.001); if (value >= 1) { value = 1; stage = Decay; } }
        else if (stage == Decay) { value -= (1 - s) / std::max(1.0, d * rate * 0.001); if (value <= s) { value = s; stage = Sustain; } }
        else if (stage == Sustain) value += 0.002 * (s - value);
        else if (stage == Release) { value -= releaseStep; if (value <= 0 || releaseStep <= 0) { value = 0; stage = Off; } }
        return value;
    }
};
inline double blep(double phase, double step) {
    if (phase < step) { const double t = phase / step; return 2 * t - t * t - 1; }
    if (phase > 1 - step) { const double t = (phase - 1) / step; return t * t + 2 * t + 1; }
    return 0;
}
inline double oscillator(double phase, double step, int wave, double pulse) {
    if (wave == 1) {
        double shifted = phase - pulse; if (shifted < 0) shifted += 1;
        return (phase < pulse ? 1.0 : -1.0) + blep(phase, step) - blep(shifted, step);
    }
    if (wave == 2) return 1 - 4 * std::abs(phase - 0.5);
    return 2 * phase - 1 - blep(phase, step);
}
struct Lowpass {
    double ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0;
    void configure(double g, double q) { a1 = 1 / (1 + g * (g + 1 / q)); a2 = g * a1; a3 = g * a2; }
    double process(double x) {
        const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return v2;
    }
};
struct Voice {
    int note = -1, channel = 0; uint64_t age = 0;
    bool pressed = false;
    double phaseA = 0, phaseB = 0, velocity = 1, pan = 0, drift = 0;
    Envelope amp, filter;
    Lowpass pole1, pole2;
};
class Engine {
public:
    void prepare(double rate) {
        sampleRate = rate; internalRate = rate * 2;
        for (auto& line : chorusLine) line.assign(static_cast<size_t>(rate * 0.04) + 4, 0);
        for (auto& ch : decimator) for (auto& f : ch) { f = {}; f.configure(std::tan(pi * rate * 0.4 / internalRate), 0.70710678); }
        panic(); lfoPhase = chorusPhase = 0; chorusWrite = 0; counter = 0; randomState = 0x12345678;
        current = Settings{};
    }
    void setSettings(const Settings& value) { target = value; }
    void noteOn(int channel, int note, double velocity) {
        if (channel < 1 || channel > 16 || note < 0 || note > 127) return;
        if (velocity <= 0) { noteOff(channel, note); return; }
        Voice* selected = nullptr;
        for (auto& v : voices) if (v.amp.stage == Envelope::Off) { selected = &v; break; }
        if (!selected) {
            for (auto& v : voices) if (!v.pressed && (!selected || v.amp.value < selected->amp.value)) selected = &v;
        }
        if (!selected) for (auto& v : voices) if (!selected || v.age < selected->age) selected = &v;
        auto& v = *selected; v = {}; v.note = note; v.channel = channel - 1; v.age = ++counter; v.pressed = true;
        v.velocity = std::clamp(velocity, 0.0, 1.0); v.phaseA = random() * 0.5 + 0.5; v.phaseB = random() * 0.5 + 0.5;
        v.drift = random(); v.pan = static_cast<double>(v.age % 5) / 2 - 1;
        v.amp.start(); v.filter.start();
    }
    void noteOff(int channel, int note) {
        Voice* found = nullptr;
        for (auto& v : voices) if (v.note == note && v.channel == channel - 1 && v.pressed && (!found || v.age < found->age)) found = &v;
        if (found) { found->pressed = false; if (!sustain[static_cast<size_t>(found->channel)]) release(*found); }
    }
    void control(int channel, int cc, int value) {
        if (channel < 1 || channel > 16) return;
        const auto c = static_cast<size_t>(channel - 1);
        if (cc == 64) {
            sustain[c] = value >= 64;
            if (!sustain[c]) for (auto& v : voices) if (v.channel == channel - 1 && !v.pressed && v.amp.stage != Envelope::Release) release(v);
        }
        if (cc == 1) wheel[c] = value / 127.0;
        if (cc == 120) { for (auto& v : voices) if (v.channel == channel - 1) v = {}; }
        if (cc == 123) for (auto& v : voices) if (v.channel == channel - 1) { v.pressed = false; release(v); }
        if (cc == 121) { sustain[c] = false; bend[c] = wheel[c] = 0; for (auto& v : voices) if (v.channel == channel - 1 && !v.pressed) release(v); }
    }
    void pitchBend(int channel, int value) { if (channel >= 1 && channel <= 16) bend[static_cast<size_t>(channel - 1)] = std::clamp((value - 8192) / 8192.0, -1.0, 1.0) * 2; }
    void panic() {
        for (auto& v : voices) v = {};
        sustain.fill(false); bend.fill(0); wheel.fill(0);
        for (auto& line : chorusLine) std::fill(line.begin(), line.end(), 0);
        // Keep prepared anti-alias coefficients while removing their tail.
        for (auto& ch : decimator) for (auto& f : ch) f.ic1 = f.ic2 = 0;
        peak = 0;
    }
    int activeVoices() const { int count = 0; for (const auto& v : voices) if (v.amp.stage != Envelope::Off) ++count; return count; }
    int heldVoices() const { int count = 0; for (const auto& v : voices) if (v.pressed) ++count; return count; }
    bool hasNote(int channel, int note) const { for (const auto& v : voices) if (v.channel == channel - 1 && v.note == note && v.amp.stage != Envelope::Off) return true; return false; }
    void render(float* left, float* right, int samples) {
        peak = 0;
        for (int i = 0; i < samples; ++i) {
            // Continuous sound-shaping controls slew to avoid automation zipper.
            const double smoothing = 1 - std::exp(-1 / (sampleRate * 0.01));
            current.cutoff += smoothing * (target.cutoff - current.cutoff); current.resonance += smoothing * (target.resonance - current.resonance);
            current.mix += smoothing * (target.mix - current.mix); current.pulse += smoothing * (target.pulse - current.pulse);
            current.chorus += smoothing * (target.chorus - current.chorus); current.volume += smoothing * (target.volume - current.volume);
            double l = 0, r = 0;
            for (int sub = 0; sub < 2; ++sub) {
                lfoPhase += target.lfoRate / internalRate; lfoPhase -= std::floor(lfoPhase);
                const double lfo = std::sin(2 * pi * lfoPhase); l = r = 0;
                for (auto& v : voices) {
                    if (v.amp.stage == Envelope::Off) continue;
                    const double env = v.amp.next(target.ampAttack, target.ampDecay, target.ampSustain, internalRate);
                    const double fenv = v.filter.next(target.filterAttack, target.filterDecay, target.filterSustain, internalRate);
                    const double midi = v.note + bend[static_cast<size_t>(v.channel)] + v.drift * target.drift * 0.05
                        + lfo * (target.vibrato + wheel[static_cast<size_t>(v.channel)] * 0.75);
                    const double frequency = 440 * std::exp2((midi - 69) / 12);
                    const double stepB = std::clamp(frequency * std::exp2((target.coarse + target.detune / 100) / 12) / internalRate, 0.000001, 0.4);
                    const double b = oscillator(v.phaseB, stepB, target.waveB, current.pulse);
                    const double stepA = std::clamp(frequency * std::exp2((b * target.polyB * 12 + fenv * target.polyEnv * 12) / 12) / internalRate, 0.000001, 0.4);
                    const double a = oscillator(v.phaseA, stepA, target.waveA, current.pulse);
                    v.phaseA += stepA; const bool wrapped = v.phaseA >= 1; v.phaseA -= std::floor(v.phaseA);
                    v.phaseB += stepB; v.phaseB -= std::floor(v.phaseB); if (target.sync && wrapped) v.phaseB = 0;
                    const double key = target.keytrack ? (v.note - 60) / 12.0 : 0;
                    const double cutoff = std::clamp(current.cutoff * std::exp2(fenv * target.filterAmount + key + lfo * target.filterLfo + (v.velocity - 1) * 0.5), 25.0, std::min(16000.0, internalRate * 0.2));
                    const double g = std::tan(pi * cutoff / internalRate);
                    v.pole1.configure(g, 0.55 + current.resonance * 4.5); v.pole2.configure(g, 0.70710678);
                    double sample = (a * (1 - current.mix) + b * current.mix) * 0.8 + random() * target.noise;
                    sample = v.pole2.process(v.pole1.process(std::tanh(sample * 1.15)));
                    sample *= env * (0.2 + 0.8 * v.velocity) * 0.28;
                    const double pan = std::clamp(v.pan * target.spread * 0.7, -1.0, 1.0);
                    l += sample * std::sqrt(0.5 * (1 - pan)); r += sample * std::sqrt(0.5 * (1 + pan));
                }
                for (auto& f : decimator[0]) l = f.process(l);
                for (auto& f : decimator[1]) r = f.process(r);
            }
            chorusLine[0][chorusWrite] = l; chorusLine[1][chorusWrite] = r;
            chorusPhase += 0.25 / sampleRate; chorusPhase -= std::floor(chorusPhase);
            const double wetL = readChorus(0, sampleRate * (0.012 + 0.003 * std::sin(2 * pi * chorusPhase)));
            const double wetR = readChorus(1, sampleRate * (0.012 + 0.003 * std::sin(2 * pi * (chorusPhase + 0.25))));
            l += current.chorus * (wetL - l); r += current.chorus * (wetR - r);
            chorusWrite = (chorusWrite + 1) % chorusLine[0].size();
            const double volume = std::pow(10.0, current.volume / 20);
            l = std::tanh(l * volume); r = std::tanh(r * volume);
            left[i] = static_cast<float>(l); if (right) right[i] = static_cast<float>(r);
            peak = std::max({peak, std::abs(l), std::abs(r)});
        }
    }
    double peak = 0;
private:
    void release(Voice& v) { v.amp.stop(target.ampRelease, internalRate); v.filter.stop(target.filterRelease, internalRate); }
    double random() { randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5; return randomState / 2147483648.0 - 1; }
    double readChorus(size_t channel, double delay) const {
        double position = static_cast<double>(chorusWrite) - delay; if (position < 0) position += chorusLine[channel].size();
        const auto first = static_cast<size_t>(position), next = (first + 1) % chorusLine[channel].size();
        return chorusLine[channel][first] + (position - first) * (chorusLine[channel][next] - chorusLine[channel][first]);
    }
    double sampleRate = 48000, internalRate = 96000, lfoPhase = 0, chorusPhase = 0;
    uint64_t counter = 0; uint32_t randomState = 0x12345678;
    size_t chorusWrite = 0;
    Settings target, current;
    std::array<Voice, 5> voices;
    std::array<bool, 16> sustain {};
    std::array<double, 16> bend {}, wheel {};
    std::array<std::vector<double>, 2> chorusLine;
    std::array<std::array<Lowpass, 2>, 2> decimator;
};
} // namespace oracle
