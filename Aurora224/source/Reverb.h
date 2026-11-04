#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace aurora {
constexpr int parameterCount = 10;
// Mode, decay, predelay, size, diffusion, damping, bass, modulation, width, mix.
inline constexpr std::array<double, parameterCount> defaults{0., .24, .08, .5, .75, .55, .5, .35, 1., .3};
class Delay {
    std::vector<float> data;
    size_t head = 0;
public:
    void prepare(size_t n) { data.assign(n, 0.f); head = 0; }
    float read(double samples) const {
        samples = std::clamp(samples, 1., double(data.size() - 2));
        double index = double(head) - samples;
        if (index < 0.) index += double(data.size());
        auto a = size_t(index); auto b = (a + 1) % data.size();
        return data[a] + float(index - double(a)) * (data[b] - data[a]);
    }
    void push(float x) { data[head] = x; if (++head == data.size()) head = 0; }
};
class Reverb {
    double sr = 48000., phase = 0.;
    std::array<Delay, 8> tank;
    std::array<Delay, 4> diffuser;
    std::array<Delay, 2> pre;
    std::array<float, 8> low{}, damp{};
    std::array<double, parameterCount> smooth = defaults;
    double slew = 0.;
public:
    void prepare(double sampleRate, const std::array<double, parameterCount>& p = defaults) {
        sr = sampleRate; phase = 0.; smooth = p;
        slew = 1. - std::exp(-1. / (.04 * sr));
        for (auto& d : tank) d.prepare(size_t(sr * .35) + 8);
        for (auto& d : diffuser) d.prepare(size_t(sr * .04) + 8);
        for (auto& d : pre) d.prepare(size_t(sr * .51) + 8);
        low.fill(0.f); damp.fill(0.f);
    }
    void tick(float inL, float inR, float& outL, float& outR,
              const std::array<double, parameterCount>& target) {
        constexpr double pi = 3.141592653589793;
        for (int i = 0; i < parameterCount; ++i) smooth[i] += slew * (target[i] - smooth[i]);
        const auto& p = smooth;
        int mode = std::clamp(int(std::round(target[0] * 2.)), 0, 2);
        double size = .55 + p[3] * 1.15;
        double rt = .3 + p[1] * 11.7;
        double delay = p[2] * .25 * sr;
        pre[0].push(inL); pre[1].push(inR);
        float l = delay < 1. ? inL : pre[0].read(delay + 1.);
        float r = delay < 1. ? inR : pre[1].read(delay + 1.);
        constexpr double apTimes[]{.0047, .0069, .0097, .0123};
        float apGain = float(.35 + .4 * p[4]);
        for (int i = 0; i < 4; ++i) {
            float& x = (i % 2 == 0) ? l : r;
            float z = diffuser[i].read(sr * apTimes[i] * size);
            float y = z - apGain * x;
            diffuser[i].push(x + apGain * y); x = y;
        }
        constexpr double times[3][8] = {
            {.0311,.0371,.0419,.0479,.0533,.0617,.0713,.0797},
            {.0199,.0239,.0293,.0317,.0379,.0431,.0473,.0539},
            {.0137,.0179,.0211,.0253,.0299,.0337,.0391,.0437}};
        std::array<float, 8> y{};
        double cutoff = 1200. * std::pow(15., 1. - p[5]);
        float hf = float(1. - std::exp(-2. * pi * cutoff / sr));
        float lf = float(1. - std::exp(-2. * pi * 250. / sr));
        float sum = 0.f;
        for (int i = 0; i < 8; ++i) {
            double t = times[mode][i] * size;
            double mod = std::sin(phase * (1. + i * .113) + i * 1.71) * p[7] * sr * .00065;
            float z = tank[i].read(t * sr + mod);
            damp[i] += hf * (z - damp[i]);
            low[i] += lf * (damp[i] - low[i]);
            double bassRt = rt * (.5 + p[6]);
            float g = float(std::pow(10., -3. * t / rt));
            float gb = float(std::pow(10., -3. * t / bassRt));
            y[i] = g * (damp[i] - low[i]) + gb * low[i];
            if (std::abs(y[i]) < 1.e-25f) y[i] = 0.f;
            sum += y[i];
        }
        // Householder reflection is orthogonal; attenuation remains below unity.
        for (int i = 0; i < 8; ++i) {
            float input = (i % 2 == 0 ? l : r) * (i < 4 ? .25f : -.25f);
            tank[i].push(y[i] - .25f * sum + input);
        }
        float wetL = (y[0] + y[2] - y[4] - y[6]) * .7f;
        float wetR = (y[1] + y[3] - y[5] - y[7]) * .7f;
        float mid = .5f * (wetL + wetR), side = float(.5 * p[8]) * (wetL - wetR);
        outL = float(1. - p[9]) * inL + float(p[9]) * (mid + side);
        outR = float(1. - p[9]) * inR + float(p[9]) * (mid - side);
        phase += 2. * pi * .31 / sr;
        // Keep phase continuous: independent non-integer oscillator ratios avoid a short repeating pattern.
    }
};
}
