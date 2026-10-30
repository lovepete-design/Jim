#include "Lattice.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace triad {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double tau = 2.0 * pi;

float bounded(float value, float low, float high, float fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

// Deterministic phase scatter avoids a different render on every host reload.
float scatter(std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / 16777216.f;
}
} // namespace

void Lattice::prepare(double sampleRate)
{
    sampleRate_ = std::isfinite(sampleRate) && sampleRate >= 8000.0
        ? std::min(sampleRate, 768000.0) : 48000.0;
    parameterPole_ = std::exp(-static_cast<double>(controlPeriod) / (.025 * sampleRate_));
    reset();
}

void Lattice::reset()
{
    voices_ = {};
    age_ = 0;
    sustain_ = false;
    targetBend_ = bend_ = 0.f;
    parameterCountdown_ = 0;
    smooth_ = target_;
    gain_ = std::pow(10.f, smooth_.outputDb / 20.f);
    attackPole_ = static_cast<float>(std::exp(-4.605170186 / (.001 * smooth_.attackMs * sampleRate_)));
    releasePole_ = static_cast<float>(std::exp(-9.210340372 / (.001 * smooth_.releaseMs * sampleRate_)));
}

void Lattice::setParameters(const LatticeParameters& parameters)
{
    target_.diffusion = bounded(parameters.diffusion, 0.f, 1.f, .35f);
    target_.tension = bounded(parameters.tension, 0.f, 1.f, .25f);
    target_.memory = bounded(parameters.memory, 0.f, 1.f, .5f);
    target_.brightness = bounded(parameters.brightness, 0.f, 1.f, .5f);
    target_.attackMs = bounded(parameters.attackMs, .5f, 5000.f, 20.f);
    target_.releaseMs = bounded(parameters.releaseMs, 5.f, 20000.f, 700.f);
    target_.outputDb = bounded(parameters.outputDb, -60.f, 6.f, -9.f);
}

void Lattice::noteOn(int note, int velocity)
{
    if (velocity <= 0) {
        noteOff(note);
        return;
    }
    note = std::clamp(note, 0, 127);
    Voice* chosen = nullptr;
    for (auto& voice : voices_) {
        if (!voice.active) {
            chosen = &voice;
            break;
        }
    }
    if (!chosen) {
        // Release tails are stolen first; within the same class, take the
        // quietest voice. A newly struck voice gets a short onset envelope.
        chosen = &*std::min_element(voices_.begin(), voices_.end(),
            [](const Voice& a, const Voice& b) {
                if (a.held != b.held) return !a.held;
                if (a.envelope != b.envelope) return a.envelope < b.envelope;
                return a.age < b.age;
            });
    }
    *chosen = Voice {};
    chosen->active = chosen->held = true;
    chosen->note = note;
    chosen->velocity = std::pow(std::clamp(velocity, 1, 127) / 127.f, 1.3f);
    chosen->age = ++age_;
    float total = 0.f;
    for (int index = 0; index < modeCount; ++index) {
        auto& mode = chosen->modes[static_cast<std::size_t>(index)];
        const double phase = tau * scatter(static_cast<std::uint32_t>(note * 53 + index * 977 + 17));
        mode.x = std::cos(phase);
        mode.y = std::sin(phase);
        mode.energy = index == 0 ? 1.f : .22f / static_cast<float>(index + 1);
        mode.target = mode.energy;
        total += mode.energy;
        const float pan = .5f + .38f * std::sin(static_cast<float>(index) * 2.39996323f);
        mode.panLeft = std::sqrt(1.f - pan);
        mode.panRight = std::sqrt(pan);
    }
    for (auto& mode : chosen->modes) mode.energy /= total;
}

void Lattice::noteOff(int note)
{
    // MIDI note offs apply to every held instance of that key, including
    // repeated note-ons sent by hosts that do not track note identifiers.
    for (auto& voice : voices_) {
        if (voice.active && voice.note == note && voice.held) {
            voice.held = false;
            voice.sustained = sustain_;
        }
    }
}

void Lattice::sustain(bool pedalDown)
{
    sustain_ = pedalDown;
    if (!pedalDown)
        for (auto& voice : voices_) voice.sustained = false;
}

void Lattice::pitchBend(float semitones)
{
    targetBend_ = bounded(semitones, -48.f, 48.f, 0.f);
}

void Lattice::smoothParameters() noexcept
{
    const float mix = static_cast<float>(1.0 - parameterPole_);
    auto approach = [mix](float& value, float target) { value += mix * (target - value); };
    approach(smooth_.diffusion, target_.diffusion);
    approach(smooth_.tension, target_.tension);
    approach(smooth_.memory, target_.memory);
    approach(smooth_.brightness, target_.brightness);
    approach(smooth_.attackMs, target_.attackMs);
    approach(smooth_.releaseMs, target_.releaseMs);
    approach(smooth_.outputDb, target_.outputDb);
    approach(bend_, targetBend_);
    gain_ = std::pow(10.f, smooth_.outputDb / 20.f);
    attackPole_ = static_cast<float>(std::exp(-4.605170186 / (.001 * smooth_.attackMs * sampleRate_)));
    releasePole_ = static_cast<float>(std::exp(-9.210340372 / (.001 * smooth_.releaseMs * sampleRate_)));
}

void Lattice::updateVoice(Voice& voice) noexcept
{
    const double dt = static_cast<double>(controlPeriod) / sampleRate_;
    const double fundamental = 440.0 * std::exp2((voice.note - 69.0 + bend_) / 12.0);
    const double memorySeconds = .018 + 1.8 * smooth_.memory * smooth_.memory;
    const float reaction = static_cast<float>(-std::expm1(-dt / memorySeconds));
    // For the ring Laplacian this bound is a convex combination, so no
    // diffusion setting or sample rate can produce negative energy.
    const float diffusion = static_cast<float>(std::min(.24, dt * (1.0 + 130.0 * smooth_.diffusion)));
    std::array<float, modeCount> nextEnergy {};
    float total = 0.f;
    for (int index = 0; index < modeCount; ++index) {
        const auto& mode = voice.modes[static_cast<std::size_t>(index)];
        const auto& before = voice.modes[static_cast<std::size_t>((index + modeCount - 1) % modeCount)];
        const auto& after = voice.modes[static_cast<std::size_t>((index + 1) % modeCount)];
        // Cross/dot products are sin/cos of phase differences, avoiding an
        // inverse trigonometric function and preserving bounded dynamics.
        const float phaseAgreement = static_cast<float>(.5 + .25 * (mode.x * before.x + mode.y * before.y
                                                                    + mode.x * after.x + mode.y * after.y));
        const float slope = 2.1f - 1.65f * smooth_.brightness;
        const float spectralSeed = std::pow(static_cast<float>(index + 1), -slope);
        const float preferred = spectralSeed * (.08f + .92f * phaseAgreement * phaseAgreement);
        nextEnergy[static_cast<std::size_t>(index)] = std::max(1.e-8f,
            mode.energy + diffusion * (before.energy + after.energy - 2.f * mode.energy)
                        + reaction * (preferred - mode.energy));
        total += nextEnergy[static_cast<std::size_t>(index)];
    }
    float amplitudeTotal = 0.f;
    for (int index = 0; index < modeCount; ++index) {
        auto& mode = voice.modes[static_cast<std::size_t>(index)];
        const auto& before = voice.modes[static_cast<std::size_t>((index + modeCount - 1) % modeCount)];
        const auto& after = voice.modes[static_cast<std::size_t>((index + 1) % modeCount)];
        mode.energy = nextEnergy[static_cast<std::size_t>(index)] / total;
        const double harmonic = static_cast<double>(index + 1);
        const double stretch = 1.0 + .008 * smooth_.tension * harmonic * harmonic;
        const double neighborTorque = (after.y * mode.x - after.x * mode.y)
                                    + (before.y * mode.x - before.x * mode.y);
        const double energyTorque = static_cast<double>(nextEnergy[static_cast<std::size_t>((index + 1) % modeCount)]
                                                      - nextEnergy[static_cast<std::size_t>((index + modeCount - 1) % modeCount)]) / total;
        // Anchor the fundamental. Other modes pull on one another in units
        // of the fundamental, making the network transpose musically.
        const double pull = index == 0 ? 0.0 : .012 * fundamental * smooth_.diffusion
                                              * (neighborTorque + 3.0 * smooth_.tension * energyTorque);
        const double frequency = std::max(1.0, fundamental * harmonic * (index == 0 ? 1.0 : stretch) + pull);
        const double rolloff = std::clamp((.43 * sampleRate_ - frequency) / (.10 * sampleRate_), 0.0, 1.0);
        const double taper = rolloff * rolloff * (3.0 - 2.0 * rolloff);
        const float untaperedAmplitude = std::sqrt(mode.energy);
        mode.amplitude = untaperedAmplitude * static_cast<float>(taper);
        // Normalize the complete spectrum before its Nyquist taper. Using
        // only audible modes here would cancel the fade on the final mode.
        amplitudeTotal += untaperedAmplitude;
        const double angle = tau * std::min(frequency, .45 * sampleRate_) / sampleRate_;
        mode.cosine = std::cos(angle);
        mode.sine = std::sin(angle);
        const double norm = 1.0 / std::sqrt(mode.x * mode.x + mode.y * mode.y);
        mode.x *= norm;
        mode.y *= norm;
    }
    const float normalization = .66f / std::max(.01f, amplitudeTotal);
    for (auto& mode : voice.modes) mode.amplitude *= normalization;
}

void Lattice::render(float* left, float* right, int count) noexcept
{
    if (!left || !right || count <= 0) return;
    for (int sample = 0; sample < count; ++sample) {
        if (parameterCountdown_-- <= 0) {
            smoothParameters();
            parameterCountdown_ = controlPeriod - 1;
        }
        double outputLeft = 0.0, outputRight = 0.0;
        for (auto& voice : voices_) {
            if (!voice.active) continue;
            if (voice.controlCountdown-- <= 0) {
                updateVoice(voice);
                voice.controlCountdown = controlPeriod - 1;
            }
            if (voice.held || voice.sustained)
                voice.envelope = 1.f + attackPole_ * (voice.envelope - 1.f);
            else {
                voice.envelope *= releasePole_;
                if (voice.envelope < 1.e-6f) {
                    voice.active = false;
                    continue;
                }
            }
            const float level = voice.envelope * voice.velocity;
            for (auto& mode : voice.modes) {
                const double signal = mode.y * mode.amplitude * level;
                outputLeft += signal * mode.panLeft;
                outputRight += signal * mode.panRight;
                const double nextX = mode.x * mode.cosine - mode.y * mode.sine;
                mode.y = mode.y * mode.cosine + mode.x * mode.sine;
                mode.x = nextX;
            }
        }
        // The fixed output saturation contains extreme 12-note chords and
        // deliberately remains quiet at ordinary patch levels.
        left[sample] = static_cast<float>(std::tanh(outputLeft * gain_));
        right[sample] = static_cast<float>(std::tanh(outputRight * gain_));
    }
}

} // namespace triad
