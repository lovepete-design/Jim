#include "PhaserDSP.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>
using namespace triadphase;
void check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
std::vector<float> tone(double frequency, double rate, double seconds = 2) {
    std::vector<float> result(static_cast<size_t>(rate * seconds));
    for (size_t i = 0; i < result.size(); ++i) result[i] = static_cast<float>(0.2 * std::sin(2 * pi * frequency * i / rate));
    return result;
}
double rms(const std::vector<float>& x) {
    double sum = 0; for (size_t i = x.size() / 2; i < x.size(); ++i) sum += x[i] * x[i];
    return std::sqrt(sum / (x.size() - x.size() / 2));
}
void render(Engine& engine, std::vector<float>& left, std::vector<float>& right, const Settings& s, int block = 127) {
    for (size_t offset = 0; offset < left.size(); offset += static_cast<size_t>(block)) {
        float* channels[] {left.data() + offset, right.data() + offset};
        engine.process(channels, 2, static_cast<int>(std::min(static_cast<size_t>(block), left.size() - offset)), s);
    }
}
int main() {
    check(std::abs(tempoRate(120, 2) - 2) < 1e-12, "Quarter-note sync at 120 BPM");
    check(std::abs(tempoRate(120, 4) - 0.5) < 1e-12, "Bar sync at 120 BPM");
    for (double rate : {44100., 48000., 96000., 192000.}) {
        const double centre = std::sqrt(160.0 * 1800);
        Settings s; s.depth = 0; Engine engine;
        // Four matched all-pass sections yield cancellation at total phases pi
        // and 3*pi. Use the exact bilinear-warped notch frequencies.
        for (double angle : {pi / 8, 3 * pi / 8}) {
            const double notch = rate / pi * std::atan(std::tan(pi * centre / rate) * std::tan(angle));
            engine.prepare(rate); auto left = tone(notch, rate), right = left; const double original = rms(left);
            render(engine, left, right, s);
            check(rms(left) / original < 0.003, "Four-stage path creates the two expected cancellation notches");
        }
        engine.prepare(rate); auto left = tone(centre, rate), right = left; const double original = rms(left);
        render(engine, left, right, s);
        check(rms(left) / original > 0.98, "Four-stage centre remains between the two notches");
        s.twoStages = true; engine.prepare(rate); left = tone(centre, rate); right = left;
        render(engine, left, right, s);
        check(rms(left) / original < 0.003, "Two-stage mode produces its centre notch");
        s = {}; s.depth = 0; s.mix = 1; engine.prepare(rate);
        left = tone(997, rate); right = left; const double wetOriginal = rms(left);
        render(engine, left, right, s);
        check(std::abs(rms(left) / wetOriginal - 1) < 0.002, "Wet-only all-pass path preserves steady-state amplitude");
        s = {}; engine.prepare(rate); left = tone(440, rate); right = left;
        render(engine, left, right, s);
        check(left == right, "Classic zero-spread processing remains stereo linked");
        s.stereoDegrees = 180; engine.prepare(rate); left = tone(440, rate); right = left;
        render(engine, left, right, s);
        double difference = 0; for (size_t i = left.size() / 2; i < left.size(); ++i) difference += std::abs(left[i] - right[i]);
        check(difference > 1, "Stereo spread offsets the channel sweeps");
        check(engine.frequency[0] >= 159 && engine.frequency[0] <= 1801, "Sweep remains in its designed range");
        s = {}; s.mix = 0; engine.prepare(rate);
        left.assign(static_cast<size_t>(rate * 0.5), 0); right = left; render(engine, left, right, s);
        left.assign(512, 0); right = left; left[0] = right[0] = 0.25f; render(engine, left, right, s);
        check(std::abs(left[0] - 0.25f) < 1e-6 && std::abs(left[1]) < 1e-6, "Dry mix has exactly zero added sample delay");
        s.bypass = true; s.mix = 0.5; s.resonant = true; s.feedback = 0.7;
        left.assign(static_cast<size_t>(rate), 0); right = left; render(engine, left, right, s);
        left.assign(512, 0); right = left; left[0] = right[0] = 0.25f; render(engine, left, right, s);
        check(std::abs(left[0] - 0.25f) < 1e-6, "Bypass keeps dry audio aligned with zero latency");
        std::mt19937 random(91); std::uniform_real_distribution<float> noise(-0.5f, 0.5f);
        engine.prepare(rate); left.resize(static_cast<size_t>(rate)); right.resize(left.size());
        for (auto& sample : left) sample = noise(random);
        right = left;
        s = {}; s.feedback = 0.7; s.resonant = true; s.rate = 10; s.stereoDegrees = 180;
        render(engine, left, right, s);
        for (size_t i = 0; i < left.size(); ++i) check(std::isfinite(left[i]) && std::isfinite(right[i]) && std::abs(left[i]) < 8,
            "Maximum resonance and sweep rate remain stable");
    }
    // Sample-wise modulation must not depend on the host's block size.
    Engine a, b; a.prepare(48000); b.prepare(48000); Settings s; s.resonant = true;
    auto l1 = tone(751, 48000), r1 = l1, l2 = l1, r2 = l1;
    render(a, l1, r1, s, 1); render(b, l2, r2, s, 511);
    check(l1 == l2 && r1 == r2, "Rendering is invariant to host block size");
    std::cout << "PASS: four/two-stage cancellation, all-pass response, tempo rates, stereo, zero delay, feedback stability, and block invariance at four sample rates\n";
}
