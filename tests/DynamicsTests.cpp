#include "plugins/stress/Dynamics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Dynamics: ") + message);
}

double settledLevel(triad::DynamicsParameters p, double sampleRate, double input) {
    triad::Dynamics processor;
    processor.prepare(sampleRate);
    processor.setParameters(p);
    std::array<float, 256> left {}, right {};
    const int blocks = static_cast<int>(sampleRate * 0.5 / left.size()) + 1;
    for (int block = 0; block < blocks; ++block) {
        left.fill(static_cast<float>(input)); right = left;
        processor.process(left.data(), right.data(), static_cast<int>(left.size()));
    }
    require(std::isfinite(processor.gainReductionDb()), "meter must remain finite");
    return left.back();
}

double sineReduction(bool highpass, bool emphasis, double frequency) {
    triad::Dynamics processor;
    processor.prepare(48000);
    triad::DynamicsParameters p;
    p.attackMs = 1; p.releaseMs = 100; p.ratio = 6;
    p.detectorHighpass = highpass; p.emphasis = emphasis;
    processor.setParameters(p);
    std::array<float, 256> buffer {};
    for (int block = 0; block < 120; ++block) {
        for (int i = 0; i < 256; ++i)
            buffer[static_cast<std::size_t>(i)] = static_cast<float>(0.5 * std::sin(2 * pi * frequency * (block * 256 + i) / 48000));
        processor.process(buffer.data(), nullptr, 256);
    }
    return processor.gainReductionDb();
}

std::array<double, 2> harmonics(int distortion) {
    triad::Dynamics processor;
    processor.prepare(48000);
    triad::DynamicsParameters p;
    p.ratio = 1; p.distortion = distortion;
    processor.setParameters(p);
    std::vector<float> buffer(48000);
    for (std::size_t i = 0; i < buffer.size(); ++i)
        buffer[i] = static_cast<float>(0.6 * std::sin(2 * pi * 1000 * i / 48000));
    processor.process(buffer.data(), nullptr, static_cast<int>(buffer.size()));
    std::array<double, 2> amplitude {};
    for (int harmonic = 2; harmonic <= 3; ++harmonic) {
        double real = 0, imag = 0;
        for (int i = 24000; i < 48000; ++i) {
            const double phase = 2 * pi * harmonic * 1000 * i / 48000;
            real += buffer[static_cast<std::size_t>(i)] * std::cos(phase);
            imag += buffer[static_cast<std::size_t>(i)] * std::sin(phase);
        }
        amplitude[static_cast<std::size_t>(harmonic - 2)] = std::hypot(real, imag) / 12000.0;
    }
    return amplitude;
}
} // namespace

void testDynamics() {
    triad::DynamicsParameters p;
    p.attackMs = 0.1f; p.releaseMs = 30;

    // A DC detector test gives an unambiguous static transfer curve.
    double previous = 1.0;
    for (float ratio : {1.f, 2.f, 3.f, 4.f, 6.f, 10.f, 20.f, 100.f}) {
        p.ratio = ratio;
        const double output = settledLevel(p, 48000, 0.5);
        const double overDb = 20 * std::log10(0.5) + 18;
        const double expected = 0.5 * std::pow(10, -(1.0 - 1.0 / ratio) * overDb / 20);
        require(std::abs(output - expected) < 0.0002, "static curve differs from ratio");
        require(output <= previous + 1.0e-6, "increasing ratio must reduce output");
        previous = output;
    }

    p.ratio = 4; p.mix = 0;
    require(std::abs(settledLevel(p, 44100, 0.25) - 0.25) < 1.0e-6, "dry mix must pass signal");
    p.mix = 1; p.ratio = 1; p.inputDb = 6; p.outputDb = -6;
    require(std::abs(settledLevel(p, 44100, 0.25) - 0.25) < 1.0e-6, "gain staging must cancel");
    p = {}; p.attackMs = 0.1f; p.releaseMs = 30;

    triad::Dynamics linked;
    linked.prepare(48000); linked.setParameters(p);
    std::array<float, 256> left {}, right {};
    for (int block = 0; block < 100; ++block) {
        left.fill(0.5f); right.fill(0.05f);
        linked.process(left.data(), right.data(), 256);
    }
    require(std::abs(left.back() / right.back() - 10) < 1.0e-4, "stereo link must preserve image");
    const float compressedMeter = linked.gainReductionDb();
    left.fill(0); right.fill(0);
    for (int block = 0; block < 200; ++block) linked.process(left.data(), right.data(), 256);
    require(compressedMeter > 5 && linked.gainReductionDb() < 0.01, "gain must release during silence");

    // Changing callback sizes must not change the sound or envelope.
    std::vector<float> whole(12000), fragmented(12000);
    for (std::size_t i = 0; i < whole.size(); ++i)
        whole[i] = fragmented[i] = static_cast<float>(0.6 * std::sin(2 * pi * 200 * i / 48000));
    triad::Dynamics first, second;
    p.distortion = 1; p.detectorHighpass = true;
    first.prepare(48000); second.prepare(48000);
    first.setParameters(p); second.setParameters(p);
    first.process(whole.data(), nullptr, static_cast<int>(whole.size()));
    for (std::size_t offset = 0; offset < fragmented.size();) {
        const int length = static_cast<int>(std::min<std::size_t>(1 + offset % 173, fragmented.size() - offset));
        second.process(fragmented.data() + offset, nullptr, length);
        offset += static_cast<std::size_t>(length);
    }
    for (std::size_t i = 0; i < whole.size(); ++i)
        require(std::abs(whole[i] - fragmented[i]) < 1.0e-7f, "callback size changed rendering");

    require(sineReduction(false, false, 40) > sineReduction(true, false, 40) + 3,
            "detector highpass must reduce bass triggering");
    require(sineReduction(false, true, 8000) > sineReduction(false, false, 8000) + 3,
            "emphasis must increase high frequency triggering");
    const auto h2 = harmonics(1), h3 = harmonics(2);
    require(h2[0] > 0.025, "H2 mode must introduce second harmonic");
    require(h3[1] > 0.015 && h3[0] < 0.0001, "H3 mode must retain odd symmetry");

    for (double sampleRate : {8000., 44100., 48000., 96000., 192000.}) {
        triad::Dynamics stress;
        stress.prepare(sampleRate);
        p.inputDb = 36; p.outputDb = 36; p.ratio = 100; p.distortion = 1;
        p.attackMs = 0.03f; p.releaseMs = 3500; p.emphasis = true;
        stress.setParameters(p);
        for (int block = 0; block < 12; ++block) {
            for (std::size_t i = 0; i < left.size(); ++i) {
                left[i] = i % 3 == 0 ? std::numeric_limits<float>::infinity() : (i % 2 ? 1000.f : -1000.f);
                right[i] = std::numeric_limits<float>::quiet_NaN();
            }
            stress.process(left.data(), right.data(), 256);
            for (std::size_t i = 0; i < left.size(); ++i)
                require(std::isfinite(left[i]) && std::isfinite(right[i]), "invalid/hot input poisoned processing");
        }
        stress.reset();
        left.fill(0); right.fill(0); stress.process(left.data(), right.data(), 256);
        for (float x : left) require(x == 0, "reset must clear distortion history");
    }
}
