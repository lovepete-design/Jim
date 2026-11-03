#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace triad {
constexpr double pi = 3.14159265358979323846;
enum class Scale { Chromatic, Major, Minor, Pentatonic };
struct Settings {
    int root = 0;
    Scale scale = Scale::Chromatic;
    float retuneMs = 30, strength = 1, humanizeCents = 8, mix = 1, outputDb = 0;
};

inline double nearestNote(double midi, int root, Scale scale) {
    constexpr std::array<unsigned, 4> masks { 0xfff, 0xab5, 0x5ad, 0x295 };
    const unsigned mask = masks[std::clamp(static_cast<int>(scale), 0, 3)];
    double best = std::round(midi), distance = 1e9;
    for (int note = static_cast<int>(std::floor(midi)) - 12;
         note <= static_cast<int>(std::ceil(midi)) + 12; ++note) {
        const auto degree = ((note - root) % 12 + 12) % 12;
        if ((mask & (1u << degree)) != 0 && std::abs(note - midi) < distance) {
            best = note; distance = std::abs(note - midi);
        }
    }
    return best;
}

// Fixed-storage YIN detector. Called on a low-pass filtered, decimated signal.
class Detector {
public:
    void prepare(double rate) { sampleRate = rate; samples.fill(0); write = count = hop = 0; frequency = confidence = 0; }
    bool push(float value) {
        samples[write] = value; write = (write + 1) % size; count = std::min(count + 1, size);
        if (++hop < 128 || count < size) return false;
        hop = 0; analyse(); return true;
    }
    double frequency = 0, confidence = 0;
private:
    static constexpr int size = 1024, half = size / 2;
    std::array<float, size> samples {};
    std::array<double, half> difference {};
    int write = 0, count = 0, hop = 0;
    double sampleRate = 12000;
    void analyse() {
        double energy = 0;
        for (auto s : samples) energy += s * s;
        frequency = confidence = 0;
        if (energy / size < 0.00001) return;
        const int maxLag = std::min(half - 2, static_cast<int>(sampleRate / 65));
        const int minLag = std::max(2, static_cast<int>(sampleRate / 1000));
        double cumulative = 0;
        difference[0] = 1;
        for (int tau = 1; tau <= maxLag + 1; ++tau) {
            double sum = 0;
            for (int j = 0; j < half; ++j) {
                const double delta = samples[(write + j) % size] - samples[(write + j + tau) % size];
                sum += delta * delta;
            }
            cumulative += sum;
            difference[tau] = cumulative > 1e-20 ? sum * tau / cumulative : 1;
        }
        for (int tau = minLag; tau < maxLag; ++tau) {
            if (difference[tau] >= 0.15) continue;
            while (tau < maxLag && difference[tau + 1] < difference[tau]) ++tau;
            const double a = difference[tau - 1], b = difference[tau], c = difference[tau + 1];
            const double denominator = a - 2 * b + c;
            const double offset = std::abs(denominator) > 1e-12 ? std::clamp(0.5 * (a - c) / denominator, -0.5, 0.5) : 0;
            frequency = sampleRate / (tau + offset); confidence = 1 - b;
            return;
        }
    }
};

// Two complementary Hann-windowed delay taps; stereo channels share phase.
// No allocation, locking, or OS calls in process(). Not formant preserving.
class Engine {
public:
    void prepare(double rate) {
        sampleRate = rate;
        decimation = std::max(1, static_cast<int>(rate / 12000));
        detector.prepare(rate / decimation);
        window = static_cast<int>(std::round(rate * 0.04));
        minimumDelay = 32;
        latency = minimumDelay + window / 2;
        for (auto& channel : delay) channel.assign(static_cast<size_t>(window + 64), 0);
        write = decimationCount = 0; phase = 0; low1 = low2 = 0;
        correction = desiredCorrection = 0; inputMidi = targetMidi = -1;
        confidence = 0; mixSmooth = gainSmooth = 1;
        filterCoefficient = 1 - std::exp(-2 * pi * 1600 / rate);
    }
    int getLatency() const { return latency; }
    double inputMidi = -1, targetMidi = -1, confidence = 0;
    void process(float* const* channels, int numChannels, int numSamples, const Settings& settings) {
        const double retune = settings.retuneMs <= 0 ? 1 : 1 - std::exp(-1 / (sampleRate * settings.retuneMs * 0.001));
        const double smoothing = 1 - std::exp(-1 / (sampleRate * 0.01));
        const double outputGain = std::pow(10.0, std::clamp(settings.outputDb, -24.f, 12.f) / 20);
        for (int i = 0; i < numSamples; ++i) {
            // Use the first channel for tracking to avoid stereo phase cancellation.
            double sample = std::isfinite(channels[0][i]) ? channels[0][i] : 0;
            low1 += filterCoefficient * (sample - low1); low2 += filterCoefficient * (low1 - low2);
            if (++decimationCount >= decimation) {
                decimationCount = 0;
                if (detector.push(static_cast<float>(low2))) {
                    confidence = detector.confidence;
                    if (detector.frequency > 0 && confidence > 0.8) {
                        inputMidi = 69 + 12 * std::log2(detector.frequency / 440);
                        targetMidi = nearestNote(inputMidi, settings.root, settings.scale);
                        const double difference = targetMidi - inputMidi;
                        const double deadband = std::clamp(settings.humanizeCents, 0.f, 50.f) / 100;
                        desiredCorrection = std::copysign(std::max(0.0, std::abs(difference) - deadband), difference)
                                            * std::clamp(settings.strength, 0.f, 1.f);
                    } else { inputMidi = targetMidi = -1; desiredCorrection = 0; }
                }
            }
            correction += retune * (desiredCorrection - correction);
            const double ratio = std::pow(2.0, correction / 12);
            phase += (1 - ratio) / window; phase -= std::floor(phase);
            const double secondPhase = phase < 0.5 ? phase + 0.5 : phase - 0.5;
            const double weight = 0.5 - 0.5 * std::cos(2 * pi * phase);
            mixSmooth += smoothing * (std::clamp(settings.mix, 0.f, 1.f) - mixSmooth);
            gainSmooth += smoothing * (outputGain - gainSmooth);
            for (int ch = 0; ch < numChannels; ++ch) {
                auto& line = delay[static_cast<size_t>(ch)];
                line[write] = std::isfinite(channels[ch][i]) ? channels[ch][i] : 0;
                const double dry = read(line, latency);
                const double wet = weight * read(line, minimumDelay + phase * window)
                                 + (1 - weight) * read(line, minimumDelay + secondPhase * window);
                channels[ch][i] = static_cast<float>((dry + mixSmooth * (wet - dry)) * gainSmooth);
            }
            write = (write + 1) % static_cast<int>(delay[0].size());
        }
    }
private:
    double sampleRate = 48000, phase = 0, correction = 0, desiredCorrection = 0;
    double low1 = 0, low2 = 0, filterCoefficient = 0, mixSmooth = 1, gainSmooth = 1;
    int decimation = 4, decimationCount = 0, window = 1920, minimumDelay = 32, latency = 992, write = 0;
    std::array<std::vector<float>, 2> delay;
    Detector detector;
    double read(const std::vector<float>& line, double delaySamples) const {
        double index = write - delaySamples;
        if (index < 0) index += line.size();
        const auto first = static_cast<size_t>(index);
        const auto next = (first + 1) % line.size();
        const double fraction = index - first;
        return line[first] + fraction * (line[next] - line[first]);
    }
};
} // namespace triad
