#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace triadfx {
constexpr double pi = 3.14159265358979323846;
inline double gain(double db) { return std::pow(10.0, db / 20.0); }
inline double db(double amplitude) { return 20 * std::log10(std::max(1e-9, amplitude)); }
inline double coefficient(double ms, double rate) { return std::exp(-1.0 / (std::max(0.01, ms) * 0.001 * rate)); }
inline double finite(double v) { return std::isfinite(v) ? std::clamp(v, -32.0, 32.0) : 0; }
struct Meters { float input = -180, output = -180, rms = -180, reduction = 0, limiting = 0, detector = -180; };

struct Coefficients { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
enum class Filter { Highpass, Bandpass, Bell, LowShelf, HighShelf };
inline Coefficients design(Filter kind, double rate, double frequency, double q = 0.7071, double dbGain = 0) {
    const double w = 2 * pi * std::clamp(frequency, 10.0, rate * 0.45) / rate;
    const double c = std::cos(w), s = std::sin(w), alpha = s / (2 * q), a = gain(dbGain / 2);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    if (kind == Filter::Highpass) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; }
    if (kind == Filter::Bandpass) { b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; }
    if (kind == Filter::Bell) { b0 = 1 + alpha * a; b1 = -2 * c; b2 = 1 - alpha * a; a0 = 1 + alpha / a; a1 = -2 * c; a2 = 1 - alpha / a; }
    if (kind == Filter::LowShelf || kind == Filter::HighShelf) {
        const double beta = s * std::sqrt(2 * a);
        if (kind == Filter::LowShelf) {
            b0 = a * ((a + 1) - (a - 1) * c + beta); b1 = 2 * a * ((a - 1) - (a + 1) * c); b2 = a * ((a + 1) - (a - 1) * c - beta);
            a0 = (a + 1) + (a - 1) * c + beta; a1 = -2 * ((a - 1) + (a + 1) * c); a2 = (a + 1) + (a - 1) * c - beta;
        } else {
            b0 = a * ((a + 1) + (a - 1) * c + beta); b1 = -2 * a * ((a - 1) + (a + 1) * c); b2 = a * ((a + 1) + (a - 1) * c - beta);
            a0 = (a + 1) - (a - 1) * c + beta; a1 = 2 * ((a - 1) - (a + 1) * c); a2 = (a + 1) - (a - 1) * c - beta;
        }
    }
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
class Biquad {
public:
    void prepare(double rate) { current = target = {}; z1 = z2 = 0; smoothing = 1 - coefficient(5, rate); }
    void set(Coefficients value) { target = value; }
    double process(double x) {
        current.b0 += smoothing * (target.b0 - current.b0); current.b1 += smoothing * (target.b1 - current.b1);
        current.b2 += smoothing * (target.b2 - current.b2); current.a1 += smoothing * (target.a1 - current.a1);
        current.a2 += smoothing * (target.a2 - current.a2);
        const double y = current.b0 * x + z1;
        z1 = current.b1 * x - current.a1 * y + z2; z2 = current.b2 * x - current.a2 * y;
        return y;
    }
private:
    Coefficients current, target;
    double z1 = 0, z2 = 0, smoothing = 1;
};

struct DeEssSettings {
    double frequency = 6500, threshold = -24, ratio = 4, range = 9, attack = 0.5, release = 80, mix = 1, output = 0;
    bool wide = false, listen = false, bypass = false;
};
class DeEsser {
public:
    void prepare(double rate) {
        sampleRate = rate; envelope = reduction = 0; mix = 1; output = 1; bypassMix = 0; wideMix = listenMix = 0;
        low.fill(0); previous.fill(0); for (auto& f : detector) f.prepare(rate); meters = {};
    }
    void process(float* const* audio, int channels, int samples, const DeEssSettings& s) {
        if (samples <= 0 || channels <= 0) return;
        const auto coefficients = design(Filter::Bandpass, sampleRate, s.frequency, 1.5);
        for (auto& f : detector) f.set(coefficients);
        const double frequency = std::clamp(s.frequency * 0.6, 20.0, sampleRate * 0.4);
        const double tangent = std::tan(pi * frequency / sampleRate), pole = (1 - tangent) / (1 + tangent);
        const double attack = coefficient(s.attack, sampleRate), release = coefficient(s.release, sampleRate), smooth = 1 - coefficient(5, sampleRate);
        double inputPeak = 0, outputPeak = 0, energy = 0, maximumReduction = 0, detectorPeak = 0;
        for (int i = 0; i < samples; ++i) {
            std::array<double, 2> x {}, band {}, high {};
            double detected = 0;
            for (int ch = 0; ch < channels; ++ch) {
                const auto c = static_cast<size_t>(ch); x[c] = finite(audio[ch][i]);
                band[c] = detector[c].process(x[c]); detected = std::max(detected, std::abs(band[c]));
                low[c] = (1 - pole) * 0.5 * (x[c] + previous[c]) + pole * low[c]; previous[c] = x[c];
                high[c] = x[c] - low[c]; inputPeak = std::max(inputPeak, std::abs(x[c]));
            }
            const double envCoefficient = detected > envelope ? attack : release;
            envelope = detected + envCoefficient * (envelope - detected);
            const double desired = std::min(s.range, std::max(0.0, db(envelope) - s.threshold) * (1 - 1 / std::max(1.0, s.ratio)));
            const double grCoefficient = desired > reduction ? attack : release;
            reduction = desired + grCoefficient * (reduction - desired);
            const double reducedGain = gain(-reduction);
            mix += smooth * (s.mix - mix); output += smooth * (gain(s.output) - output);
            bypassMix += smooth * ((s.bypass ? 1.0 : 0.0) - bypassMix);
            wideMix += smooth * ((s.wide ? 1.0 : 0.0) - wideMix); listenMix += smooth * ((s.listen ? 1.0 : 0.0) - listenMix);
            for (int ch = 0; ch < channels; ++ch) {
                const auto c = static_cast<size_t>(ch);
                const double split = x[c] + (reducedGain - 1) * high[c];
                double wet = split + wideMix * (x[c] * reducedGain - split);
                wet = (x[c] + mix * (wet - x[c])) * output;
                wet += listenMix * (band[c] - wet);
                const double y = wet + bypassMix * (x[c] - wet);
                audio[ch][i] = static_cast<float>(y); outputPeak = std::max(outputPeak, std::abs(y)); energy += y * y;
            }
            maximumReduction = std::max(maximumReduction, reduction); detectorPeak = std::max(detectorPeak, envelope);
        }
        meters = {static_cast<float>(db(inputPeak)), static_cast<float>(db(outputPeak)), static_cast<float>(db(std::sqrt(energy / (samples * channels)))),
            static_cast<float>(maximumReduction), 0, static_cast<float>(db(detectorPeak))};
    }
    Meters meters;
private:
    double sampleRate = 48000, envelope = 0, reduction = 0, mix = 1, output = 1, bypassMix = 0, wideMix = 0, listenMix = 0;
    std::array<double, 2> low {}, previous {};
    std::array<Biquad, 2> detector;
};

// Fixed-capacity monotonic queue finds the largest peak over the look-ahead
// window. Gain can never exceed the bound required by the delayed sample.
class PeakLimiter {
public:
    void prepare(double rate) {
        delay = std::max(1, static_cast<int>(std::round(rate * 0.002)));
        for (auto& line : audio) line.assign(static_cast<size_t>(delay + 1), 0);
        peaks.assign(static_cast<size_t>(delay + 2), 0); indices.assign(peaks.size(), 0);
        head = tail = write = 0; time = 0; limiterGain = 1; maximumReduction = 0; release = coefficient(100, rate);
    }
    int latency() const { return delay; }
    std::array<double, 2> process(std::array<double, 2> input, double ceiling, bool enabled) {
        const auto capacity = peaks.size();
        const double peak = std::max(std::abs(input[0]), std::abs(input[1]));
        while (head != tail && indices[head] + static_cast<uint64_t>(delay) < time) head = (head + 1) % capacity;
        while (head != tail) {
            const auto last = (tail + capacity - 1) % capacity;
            if (peaks[last] > peak) break;
            tail = last;
        }
        peaks[tail] = peak; indices[tail] = time; tail = (tail + 1) % capacity;
        const double target = std::min(1.0, ceiling / std::max(peaks[head], 1e-12));
        limiterGain = std::min(target, 1 + release * (limiterGain - 1));
        std::array<double, 2> result {};
        const auto read = (write + 1) % audio[0].size();
        for (size_t ch = 0; ch < 2; ++ch) {
            audio[ch][write] = input[ch];
            result[ch] = audio[ch][read] * (enabled ? limiterGain : 1.0);
            if (enabled) result[ch] = std::clamp(result[ch], -ceiling, ceiling);
        }
        write = read; ++time;
        maximumReduction = enabled ? std::max(maximumReduction, -db(limiterGain)) : maximumReduction;
        return result;
    }
    double maximumReduction = 0;
private:
    int delay = 96;
    size_t head = 0, tail = 0, write = 0;
    uint64_t time = 0;
    double limiterGain = 1, release = 0;
    std::array<std::vector<double>, 2> audio;
    std::vector<double> peaks;
    std::vector<uint64_t> indices;
};
struct MasterSettings {
    double input = 0, highpass = 25, low = 0, midFrequency = 1000, mid = 0, high = 0;
    double threshold = -18, ratio = 1.5, attack = 30, release = 150, makeup = 0;
    double saturation = 0, width = 1, monoBass = 100, loudness = 0, ceiling = -1;
    bool eq = true, compressor = true, limiter = true, bypass = false;
};
class Mastering {
public:
    void prepare(double rate) {
        sampleRate = rate; envelope = reduction = sideLow = sidePrevious = 0; inputGain = outputGain = width = 1; saturation = bypassMix = 0;
        for (auto& channel : eq) for (auto& filter : channel) filter.prepare(rate);
        limiter.prepare(rate); for (auto& ch : dry) ch.assign(static_cast<size_t>(limiter.latency() + 1), 0);
        dryWrite = 0; meters = {};
    }
    int latency() const { return limiter.latency(); }
    void process(float* const* audio, int channels, int samples, const MasterSettings& s) {
        if (samples <= 0 || channels <= 0) return;
        for (auto& channel : eq) {
            channel[0].set(s.eq ? design(Filter::Highpass, sampleRate, s.highpass) : Coefficients{});
            channel[1].set(s.eq ? design(Filter::LowShelf, sampleRate, 120, 0.7071, s.low) : Coefficients{});
            channel[2].set(s.eq ? design(Filter::Bell, sampleRate, s.midFrequency, 0.8, s.mid) : Coefficients{});
            channel[3].set(s.eq ? design(Filter::HighShelf, sampleRate, 9000, 0.7071, s.high) : Coefficients{});
        }
        const double attack = coefficient(s.attack, sampleRate), release = coefficient(s.release, sampleRate), smooth = 1 - coefficient(10, sampleRate);
        const double rmsCoefficient = coefficient(5, sampleRate);
        const double tangent = std::tan(pi * std::clamp(s.monoBass, 20.0, sampleRate * 0.4) / sampleRate), sidePole = (1 - tangent) / (1 + tangent);
        double inputPeak = 0, outputPeak = 0, energy = 0, maximumReduction = 0;
        limiter.maximumReduction = 0;
        for (int i = 0; i < samples; ++i) {
            inputGain += smooth * (gain(s.input) - inputGain);
            outputGain += smooth * (gain(s.makeup + s.loudness) - outputGain);
            saturation += smooth * (s.saturation - saturation); width += smooth * (s.width - width);
            bypassMix += smooth * ((s.bypass ? 1.0 : 0.0) - bypassMix);
            std::array<double, 2> x {}, original {};
            const auto dryRead = (dryWrite + 1) % dry[0].size();
            for (int ch = 0; ch < 2; ++ch) {
                const auto c = static_cast<size_t>(ch);
                const double value = ch < channels ? finite(audio[ch][i]) : 0;
                dry[c][dryWrite] = value; original[c] = dry[c][dryRead];
                inputPeak = std::max(inputPeak, std::abs(value)); x[c] = value * inputGain;
                for (auto& filter : eq[c]) x[c] = filter.process(x[c]);
            }
            const double square = std::max(x[0] * x[0], x[1] * x[1]);
            envelope = square + rmsCoefficient * (envelope - square);
            const double over = db(std::sqrt(envelope)) - s.threshold, slope = 1 - 1 / std::max(1.0, s.ratio);
            const double desired = !s.compressor || over <= -3 ? 0 : over >= 3 ? slope * over : slope * (over + 3) * (over + 3) / 12;
            reduction = desired + (desired > reduction ? attack : release) * (reduction - desired);
            for (auto& sample : x) {
                sample *= gain(-reduction);
                const double coloured = std::tanh(sample * 2) / std::tanh(2.0);
                sample += saturation * (coloured - sample);
            }
            if (channels == 2) {
                const double mid = (x[0] + x[1]) * 0.5, side = (x[0] - x[1]) * 0.5;
                sideLow = (1 - sidePole) * 0.5 * (side + sidePrevious) + sidePole * sideLow; sidePrevious = side;
                const double adjustedSide = (side - sideLow) * width;
                x = {mid + adjustedSide, mid - adjustedSide};
            }
            for (auto& sample : x) sample *= outputGain;
            x = limiter.process(x, gain(s.ceiling), s.limiter);
            for (int ch = 0; ch < channels; ++ch) {
                const auto c = static_cast<size_t>(ch); const double value = x[c] + bypassMix * (original[c] - x[c]);
                audio[ch][i] = static_cast<float>(value); outputPeak = std::max(outputPeak, std::abs(value)); energy += value * value;
            }
            dryWrite = dryRead; maximumReduction = std::max(maximumReduction, reduction);
        }
        meters = {static_cast<float>(db(inputPeak)), static_cast<float>(db(outputPeak)), static_cast<float>(db(std::sqrt(energy / (samples * channels)))),
            static_cast<float>(maximumReduction), static_cast<float>(limiter.maximumReduction), 0};
    }
    Meters meters;
private:
    double sampleRate = 48000, envelope = 0, reduction = 0, inputGain = 1, outputGain = 1, saturation = 0, width = 1;
    double sideLow = 0, sidePrevious = 0, bypassMix = 0;
    size_t dryWrite = 0;
    std::array<std::array<Biquad, 4>, 2> eq;
    std::array<std::vector<double>, 2> dry;
    PeakLimiter limiter;
};
} // namespace triadfx
