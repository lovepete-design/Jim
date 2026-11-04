#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace triadphase {
constexpr double pi = 3.14159265358979323846;
struct Settings {
    double rate = 0.45, depth = 1, feedback = 0.35, mix = 0.5, outputDb = 0, stereoDegrees = 0;
    bool resonant = false, twoStages = false, sync = false, bypass = false;
    int division = 2;
};
inline double tempoRate(double bpm, int division) {
    constexpr std::array<double, 7> beats {0.25, 0.5, 1, 2, 4, 8, 16};
    return std::clamp(bpm, 20.0, 400.0) / (60 * beats[static_cast<size_t>(std::clamp(division, 0, 6))]);
}
class Allpass {
public:
    void reset() { state = 0; }
    double process(double input, double coefficient) {
        const double output = coefficient * input + state;
        state = input - coefficient * output;
        return output;
    }
private:
    double state = 0;
};
class Engine {
public:
    void prepare(double rate) {
        sampleRate = rate; phase = 0; frequency = {std::sqrt(160.0 * 1800), std::sqrt(160.0 * 1800)};
        for (auto& channel : stages) for (auto& stage : channel) stage.reset();
        previousWet.fill(0); mix = 0.5; depth = 1; speed = 0.45; feedback = offset = stageBlend = bypass = 0; outputGain = 1;
        smooth = 1 - std::exp(-1 / (rate * 0.01)); inputPeak = outputPeak = 0; lfo = 0.5;
    }
    void process(float* const* audio, int channels, int samples, const Settings& s, double bpm = 120) {
        if (channels < 1 || samples < 1) return;
        const double targetRate = s.sync ? tempoRate(bpm, s.division) : std::clamp(s.rate, 0.05, 10.0);
        const double targetOutput = std::pow(10.0, std::clamp(s.outputDb, -18.0, 12.0) / 20);
        inputPeak = outputPeak = 0;
        for (int i = 0; i < samples; ++i) {
            speed += smooth * (targetRate - speed); depth += smooth * (std::clamp(s.depth, 0.0, 1.0) - depth);
            mix += smooth * (std::clamp(s.mix, 0.0, 1.0) - mix);
            feedback += smooth * ((s.resonant ? std::clamp(s.feedback, 0.0, 0.7) : 0) - feedback);
            offset += smooth * (std::clamp(s.stereoDegrees, 0.0, 180.0) / 360 - offset);
            stageBlend += smooth * ((s.twoStages ? 1.0 : 0.0) - stageBlend);
            bypass += smooth * ((s.bypass ? 1.0 : 0.0) - bypass); outputGain += smooth * (targetOutput - outputGain);
            phase += speed / sampleRate; phase -= std::floor(phase);
            lfo = 0.5 + 0.5 * std::sin(2 * pi * phase);
            for (int ch = 0; ch < std::min(2, channels); ++ch) {
                const auto c = static_cast<size_t>(ch);
                const double raw = audio[ch][i]; const double dry = std::isfinite(raw) ? std::clamp(raw, -32.0, 32.0) : 0;
                const double sweep = std::sin(2 * pi * (phase + (ch == 1 ? offset : 0)));
                // Equal all-pass poles sweep exponentially, giving two moving
                // cancellation notches when the four-stage tap meets dry audio.
                const double centre = std::sqrt(160.0 * 1800);
                const double desiredFrequency = centre * std::exp(0.5 * std::log(1800.0 / 160) * depth * sweep);
                frequency[c] += smooth * (desiredFrequency - frequency[c]);
                const double t = std::tan(pi * std::clamp(frequency[c], 20.0, sampleRate * 0.4) / sampleRate);
                const double a = (t - 1) / (t + 1);
                // Bounded soft saturation in the feedback return gives a mild
                // analogue-like resonance without placing distortion in Script.
                double wet = dry + feedback * std::tanh(previousWet[c]);
                double second = 0;
                for (size_t stage = 0; stage < 4; ++stage) {
                    wet = stages[c][stage].process(wet, a);
                    if (stage == 1) second = wet;
                }
                wet += stageBlend * (second - wet); previousWet[c] = wet;
                wet *= 1 - 0.5 * feedback;
                const double processed = (dry + mix * (wet - dry)) * outputGain;
                const double value = processed + bypass * (dry - processed);
                audio[ch][i] = static_cast<float>(value);
                inputPeak = std::max(inputPeak, std::abs(dry)); outputPeak = std::max(outputPeak, std::abs(value));
            }
        }
    }
    double inputPeak = 0, outputPeak = 0, lfo = 0.5;
    std::array<double, 2> frequency {500, 500};
private:
    double sampleRate = 48000, phase = 0, speed = 0.45, mix = 0.5, depth = 1, feedback = 0, outputGain = 1;
    double offset = 0, stageBlend = 0, bypass = 0, smooth = 0;
    std::array<std::array<Allpass, 4>, 2> stages;
    std::array<double, 2> previousWet {};
};
} // namespace triadphase
