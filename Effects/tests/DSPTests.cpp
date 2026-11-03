#include "AudioDSP.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
using namespace triadfx;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::vector<float> tone(double frequency, double rate, double amplitude = 0.3) {
    std::vector<float> result(static_cast<size_t>(rate));
    for (size_t i = 0; i < result.size(); ++i) result[i] = static_cast<float>(amplitude * std::sin(2 * pi * frequency * i / rate));
    return result;
}
double rms(const std::vector<float>& values) {
    double sum = 0; for (size_t i = values.size() / 2; i < values.size(); ++i) sum += values[i] * values[i];
    return std::sqrt(sum / (values.size() - values.size() / 2));
}
template<class Engine, class Settings>
void render(Engine& engine, std::vector<float>& left, std::vector<float>& right, const Settings& settings, int block = 127) {
    for (size_t i = 0; i < left.size(); i += static_cast<size_t>(block)) {
        float* channels[] {left.data() + i, right.data() + i};
        engine.process(channels, 2, static_cast<int>(std::min(static_cast<size_t>(block), left.size() - i)), settings);
    }
}
int main() {
    for (double rate : {44100., 48000., 96000., 192000.}) {
        DeEssSettings settings; settings.frequency = 7000; settings.threshold = -30; settings.ratio = 8; settings.range = 12;
        DeEsser deess; deess.prepare(rate);
        auto low = tone(300, rate), lowR = low; const double lowRms = rms(low);
        render(deess, low, lowR, settings);
        check(std::abs(db(rms(low) / lowRms)) < 0.5, "De-esser preserves non-sibilant low tone");
        deess.prepare(rate);
        auto high = tone(7000, rate), highR = high; const double highRms = rms(high);
        render(deess, high, highR, settings);
        check(db(rms(high) / highRms) < -3, "Split de-esser attenuates sibilant tone");
        check(high == highR, "De-esser stereo gain is linked");
        settings.wide = true; deess.prepare(rate); high = tone(7000, rate); highR = high;
        render(deess, high, highR, settings);
        check(db(rms(high) / highRms) < -8, "Wideband reduction reaches configured strength");
        check(deess.meters.reduction <= settings.range + 0.01, "De-esser respects maximum reduction");
        settings = {}; settings.range = 0; deess.prepare(rate);
        std::vector<float> impulse(1024, 0), impulseR(1024, 0); impulse[0] = impulseR[0] = 0.5f;
        render(deess, impulse, impulseR, settings);
        check(impulse[0] == 0.5f && impulse[1] == 0, "De-esser adds zero samples of delay");

        Mastering master; master.prepare(rate);
        MasterSettings neutral; neutral.eq = false; neutral.compressor = false; neutral.limiter = false;
        auto source = tone(440, rate), left = source, right = source;
        render(master, left, right, neutral);
        const int latency = master.latency();
        check(latency == static_cast<int>(std::round(rate * 0.002)), "Mastering latency equals 2 ms");
        for (size_t i = 0; i < source.size(); ++i) {
            const float expected = i < static_cast<size_t>(latency) ? 0 : source[i - static_cast<size_t>(latency)];
            check(std::abs(left[i] - expected) < 1e-6, "Disabled master stages preserve delayed signal");
        }
        MasterSettings loud; loud.input = 18; loud.loudness = 18; loud.makeup = 12; loud.width = 1.5;
        loud.low = 6; loud.mid = 6; loud.high = 6; loud.saturation = 1; loud.ceiling = -1;
        master.prepare(rate); left = tone(333, rate, 0.95); right = tone(751, rate, 0.95);
        render(master, left, right, loud);
        for (size_t i = 0; i < left.size(); ++i)
            check(std::isfinite(left[i]) && std::isfinite(right[i]) && std::max(std::abs(left[i]), std::abs(right[i])) <= gain(-1) + 1e-6,
                "Mastering limiter bounds both output channels at ceiling");
        check(master.meters.limiting > 1, "Limiter reports actual reduction");
        check(master.meters.reduction > 1, "Compressor reports actual reduction");
        MasterSettings mono; mono.width = 0; mono.eq = false; mono.compressor = false;
        master.prepare(rate); left = tone(440, rate); right = tone(997, rate);
        render(master, left, right, mono);
        for (size_t i = static_cast<size_t>(rate / 2); i < left.size(); ++i)
            check(std::abs(left[i] - right[i]) < 1e-6, "Width zero produces mono output");
        // Filter targets and ceiling may change between host blocks.
        std::mt19937 random(17); std::uniform_real_distribution<float> noise(-1, 1);
        master.prepare(rate); left.resize(48000); right.resize(48000);
        for (auto& sample : left) sample = noise(random);
        for (auto& sample : right) sample = noise(random);
        for (size_t offset = 0; offset < left.size(); offset += 64) {
            loud.low = offset % 128 == 0 ? -6 : 6; loud.midFrequency = offset % 128 == 0 ? 200 : 4000;
            loud.ceiling = offset % 128 == 0 ? -3 : -0.1;
            float* ptr[] {left.data() + offset, right.data() + offset};
            const int count = static_cast<int>(std::min<size_t>(64, left.size() - offset));
            master.process(ptr, 2, count, loud);
            for (int i = 0; i < count; ++i) check(std::isfinite(ptr[0][i]) && std::abs(ptr[0][i]) <= gain(loud.ceiling) + 1e-6,
                "Automated EQ and ceiling remain finite and bounded");
        }
    }
    // The look-ahead queue must retain isolated peaks until they reach output.
    PeakLimiter limiter; limiter.prepare(48000);
    int audible = 0;
    for (int i = 0; i < 500; ++i) {
        const auto result = limiter.process({i == 0 ? 8.0 : 0.0, 0}, gain(-1), true);
        if (result[0] != 0) { check(i == limiter.latency(), "Limiter impulse arrives at advertised delay"); ++audible; }
        check(std::abs(result[0]) <= gain(-1), "Isolated peak stays under ceiling");
    }
    check(audible == 1, "Limiter retains isolated impulse");
    std::cout << "PASS: de-essing, band selectivity, range, stereo linkage, zero delay, mastering bypassed stages, 2 ms look-ahead, ceiling, compression, mono width, automation, and impulse timing at four sample rates\n";
}
