#include "Dynamics.h"

#include <algorithm>
#include <cmath>

namespace triad {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double ln10over20 = 0.1151292546497022842;

double finiteClamp(double x, double low, double high, double fallback) noexcept {
    return std::isfinite(x) ? std::clamp(x, low, high) : fallback;
}

double cleanInput(float x) noexcept {
    return std::isfinite(x) ? std::clamp(static_cast<double>(x), -32.0, 32.0) : 0.0;
}

void approach(double& value, double target, double coefficient) noexcept {
    value += coefficient * (target - value);
    if (std::abs(target - value) < 1.0e-12) value = target;
}

std::array<double, 2> primitives(double x) noexcept {
    // Stable log(cosh) avoids overflow, including unexpectedly hot host audio.
    const double z = std::abs(1.6 * x);
    const double logCosh = z + std::log1p(std::exp(-2.0 * z)) - std::log(2.0);
    return {logCosh / 2.56, (1.6 * x - std::tanh(1.6 * x)) / 2.56};
}

double dcBlock(double x, double& previousX, double& previousY, double pole) noexcept {
    const double y = x - previousX + pole * previousY;
    previousX = x;
    previousY = std::abs(y) < 1.0e-25 ? 0.0 : y;
    return y;
}
} // namespace

void Dynamics::prepare(double sampleRate) {
    sampleRate_ = finiteClamp(sampleRate, 8000.0, 384000.0, 44100.0);
    smooth_ = 1.0 - std::exp(-1.0 / (0.015 * sampleRate_));
    hpPole_ = std::exp(-2.0 * pi * 80.0 / sampleRate_);
    emphasisPole_ = std::exp(-2.0 * pi * 3000.0 / sampleRate_);
    dcPole_ = std::exp(-2.0 * pi * 12.0 / sampleRate_);
    reset();
}

void Dynamics::reset() {
    channels_ = {};
    detectorPower_ = reduction_ = releaseMemory_ = 0.0;
    meter_.store(0.0f, std::memory_order_relaxed);
    initialised_ = false;
}

void Dynamics::setParameters(const DynamicsParameters& p) {
    parameters_.inputDb = static_cast<float>(finiteClamp(p.inputDb, -36.0, 36.0, 0.0));
    parameters_.outputDb = static_cast<float>(finiteClamp(p.outputDb, -60.0, 36.0, 0.0));
    parameters_.attackMs = static_cast<float>(finiteClamp(p.attackMs, 0.03, 30.0, 5.0));
    parameters_.releaseMs = static_cast<float>(finiteClamp(p.releaseMs, 20.0, 3500.0, 100.0));
    parameters_.ratio = static_cast<float>(finiteClamp(p.ratio, 1.0, 100.0, 4.0));
    parameters_.mix = static_cast<float>(finiteClamp(p.mix, 0.0, 1.0, 1.0));
    parameters_.distortion = std::clamp(p.distortion, 0, 2);
    parameters_.detectorHighpass = p.detectorHighpass;
    parameters_.emphasis = p.emphasis;
}

void Dynamics::process(float* left, float* right, int count) noexcept {
    if (left == nullptr || count <= 0) return;
    const bool stereo = right != nullptr && right != left;
    const auto& p = parameters_;
    const double slopeTarget = 1.0 - 1.0 / static_cast<double>(p.ratio);
    const double h2Target = p.distortion == 1 ? 1.0 : 0.0;
    const double h3Target = p.distortion == 2 ? 1.0 : 0.0;
    if (!initialised_) {
        inputDb_ = p.inputDb; outputDb_ = p.outputDb;
        attackMs_ = p.attackMs; releaseMs_ = p.releaseMs;
        slope_ = slopeTarget; mix_ = p.mix;
        h2_ = h2Target; h3_ = h3Target;
        hpMix_ = p.detectorHighpass ? 1.0 : 0.0;
        emphasisMix_ = p.emphasis ? 1.0 : 0.0;
        initialised_ = true;
    }

    const double powerPole = std::exp(-1.0 / (0.0005 * sampleRate_));
    const double memoryPole = std::exp(-1.0 / (0.5 * sampleRate_));
    const double optoAmount = std::exp(-std::pow((static_cast<double>(p.ratio) - 10.0) / 0.7, 2.0));
    for (int i = 0; i < count; ++i) {
        approach(inputDb_, p.inputDb, smooth_);
        approach(outputDb_, p.outputDb, smooth_);
        approach(attackMs_, p.attackMs, smooth_);
        approach(releaseMs_, p.releaseMs, smooth_);
        approach(slope_, slopeTarget, smooth_);
        approach(mix_, p.mix, smooth_);
        approach(h2_, h2Target, smooth_);
        approach(h3_, h3Target, smooth_);
        approach(hpMix_, p.detectorHighpass ? 1.0 : 0.0, smooth_);
        approach(emphasisMix_, p.emphasis ? 1.0 : 0.0, smooth_);
        const double inputGain = std::exp(inputDb_ * ln10over20);
        const double outputGain = std::exp(outputDb_ * ln10over20);
        const double input[2] {cleanInput(left[i]) * inputGain,
                               stereo ? cleanInput(right[i]) * inputGain : 0.0};

        double detector = 0.0;
        for (int channel = 0; channel < (stereo ? 2 : 1); ++channel) {
            auto& state = channels_[static_cast<std::size_t>(channel)];
            const double hp = hpPole_ * (state.hpY + input[channel] - state.hpX);
            state.hpX = input[channel]; state.hpY = std::abs(hp) < 1.0e-25 ? 0.0 : hp;
            double sidechain = input[channel] + hpMix_ * (hp - input[channel]);
            state.emphasisLow = (1.0 - emphasisPole_) * sidechain + emphasisPole_ * state.emphasisLow;
            if (std::abs(state.emphasisLow) < 1.0e-25) state.emphasisLow = 0.0;
            sidechain += emphasisMix_ * 1.8 * (sidechain - state.emphasisLow);
            detector = std::max(detector, std::abs(sidechain));
        }
        detectorPower_ = powerPole * detectorPower_ + (1.0 - powerPole) * detector * detector;
        if (detectorPower_ < 1.0e-25) detectorPower_ = 0.0;
        // RMS/peak blend reduces waveform ripple while retaining transient sensitivity.
        const double level = 0.75 * detector + 0.25 * std::sqrt(detectorPower_);
        const double overDb = 20.0 * std::log10(std::max(level, 1.0e-12)) + 18.0;
        const double kneeDb = 6.0;
        double desired = 0.0;
        if (overDb > kneeDb * 0.5)
            desired = slope_ * overDb;
        else if (overDb > -kneeDb * 0.5)
            desired = slope_ * (overDb + kneeDb * 0.5) * (overDb + kneeDb * 0.5) / (2.0 * kneeDb);
        desired = std::clamp(desired, 0.0, 72.0);
        releaseMemory_ = memoryPole * releaseMemory_ + (1.0 - memoryPole) * reduction_;
        if (releaseMemory_ < 1.0e-25) releaseMemory_ = 0.0;
        // The 10:1 mode adds a slow, program-dependent recovery component.
        const double programFactor = 1.0 + optoAmount * std::min(releaseMemory_ / 8.0, 2.0);
        const double timeMs = desired > reduction_ ? attackMs_ : releaseMs_ * programFactor;
        const double envelopePole = std::exp(-1.0 / (0.001 * timeMs * sampleRate_));
        reduction_ = envelopePole * reduction_ + (1.0 - envelopePole) * desired;
        if (reduction_ < 1.0e-20) reduction_ = 0.0;
        const double gain = std::exp(-reduction_ * ln10over20);

        for (int channel = 0; channel < (stereo ? 2 : 1); ++channel) {
            auto& state = channels_[static_cast<std::size_t>(channel)];
            const double wet = input[channel] * gain;
            double coloured = wet;
            if (h2_ + h3_ > 1.0e-8) {
                if (!state.primitiveValid) {
                    const auto previous = primitives(state.previousWet);
                    state.previousPrimitiveOdd = previous[0];
                    state.previousPrimitiveEven = previous[1];
                }
                const auto current = primitives(wet);
                const double difference = wet - state.previousWet;
                double odd = 0.0, even = 0.0;
                if (std::abs(difference) < 1.0e-5) {
                    const double t = std::tanh(0.8 * (wet + state.previousWet));
                    odd = t / 1.6; even = t * t / 1.6;
                } else {
                    // First-order antiderivative antialiasing, with cached primitives.
                    odd = (current[0] - state.previousPrimitiveOdd) / difference;
                    even = (current[1] - state.previousPrimitiveEven) / difference;
                }
                state.previousPrimitiveOdd = current[0];
                state.previousPrimitiveEven = current[1];
                state.primitiveValid = true;
                odd = dcBlock(odd, state.oddDcX, state.oddDcY, dcPole_);
                even = dcBlock(even, state.evenDcX, state.evenDcY, dcPole_);
                coloured += (h2_ + h3_) * (odd - wet) + h2_ * 0.32 * even;
            } else {
                state.primitiveValid = false;
                state.oddDcX = state.oddDcY = state.evenDcX = state.evenDcY = 0.0;
            }
            state.previousWet = wet;
            const double output = (input[channel] + mix_ * (coloured - input[channel])) * outputGain;
            (channel == 0 ? left : right)[i] = static_cast<float>(std::clamp(output, -4096.0, 4096.0));
        }
    }
    meter_.store(static_cast<float>(reduction_), std::memory_order_relaxed);
}

float Dynamics::gainReductionDb() const noexcept {
    return meter_.load(std::memory_order_relaxed);
}
} // namespace triad
