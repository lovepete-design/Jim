#include "Lattice.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void requireLattice(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

double energy(const std::vector<float>& samples)
{
    double sum = 0.0;
    for (float value : samples) sum += static_cast<double>(value) * value;
    return sum / static_cast<double>(samples.size());
}

double projection(const std::vector<float>& samples, double frequency, double rate)
{
    constexpr double tau = 6.28318530717958647692;
    double real = 0.0, imaginary = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double phase = tau * frequency * static_cast<double>(index) / rate;
        real += samples[index] * std::cos(phase);
        imaginary -= samples[index] * std::sin(phase);
    }
    return (real * real + imaginary * imaginary) / static_cast<double>(samples.size() * samples.size());
}
} // namespace

void testLattice()
{
    using namespace triad;
    std::vector<float> left(48000), right(48000);
    Lattice synth;
    synth.prepare(48000.0);
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    requireLattice(energy(left) == 0.0 && energy(right) == 0.0, "Lattice: fresh synth must be silent");

    LatticeParameters parameters;
    parameters.attackMs = 1.f;
    parameters.releaseMs = 100.f;
    synth.setParameters(parameters);
    synth.reset();
    synth.noteOn(69, 110);
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    requireLattice(energy(left) > .00001 && energy(right) > .00001, "Lattice: note produced no signal");
    requireLattice(projection(left, 440.0, 48000.0) > 20.0 * projection(left, 450.0, 48000.0),
                   "Lattice: anchored fundamental is not MIDI A440");
    double stereoDifference = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index)
        stereoDifference += std::abs(left[index] - right[index]);
    requireLattice(stereoDifference > 1.0, "Lattice: stereo mode placement is inactive");

    synth.sustain(true);
    synth.noteOff(69);
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    requireLattice(energy(left) > .00001, "Lattice: sustain pedal failed to hold note");
    synth.sustain(false);
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    requireLattice(energy(left) == 0.0 && energy(right) == 0.0, "Lattice: released voice did not become silent");

    // Rendering must be sample-identical for host block sizes, including
    // boundaries that are not multiples of the internal control period.
    Lattice whole, divided;
    whole.prepare(48000.0);
    divided.prepare(48000.0);
    whole.noteOn(57, 100);
    divided.noteOn(57, 100);
    std::vector<float> chunkLeft(left.size()), chunkRight(right.size());
    whole.render(left.data(), right.data(), static_cast<int>(left.size()));
    for (int offset = 0; offset < static_cast<int>(left.size());) {
        const int length = std::min(127, static_cast<int>(left.size()) - offset);
        divided.render(chunkLeft.data() + offset, chunkRight.data() + offset, length);
        offset += length;
    }
    requireLattice(left == chunkLeft && right == chunkRight, "Lattice: audio depends on host block size");

    // The network controls must change the sound, not merely expose UI values.
    Lattice gentle, restless;
    gentle.prepare(48000.0);
    restless.prepare(48000.0);
    LatticeParameters a, b;
    a.diffusion = 0.f; a.tension = 0.f; a.brightness = .1f;
    b.diffusion = 1.f; b.tension = .9f; b.brightness = .9f;
    gentle.setParameters(a); gentle.reset();
    restless.setParameters(b); restless.reset();
    gentle.noteOn(57, 100); restless.noteOn(57, 100);
    gentle.render(left.data(), right.data(), static_cast<int>(left.size()));
    restless.render(chunkLeft.data(), chunkRight.data(), static_cast<int>(chunkLeft.size()));
    double change = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index)
        change += std::abs(left[index] - chunkLeft[index]);
    requireLattice(change > 100.0, "Lattice: network controls do not change the audio");

    // At 8kHz these high notes retain only their fundamental. Its level
    // must fade smoothly as it approaches the 0.43*sampleRate cutoff;
    // normalization must not cancel the anti-alias taper.
    Lattice high, higher;
    high.prepare(8000.0); higher.prepare(8000.0);
    high.setParameters(parameters); high.reset();
    higher.setParameters(parameters); higher.reset();
    high.noteOn(101, 100); higher.noteOn(104, 100);
    high.render(left.data(), right.data(), 8000);
    higher.render(chunkLeft.data(), chunkRight.data(), 8000);
    double lowerEnergy = 0.0, higherEnergy = 0.0;
    for (int index = 0; index < 8000; ++index) {
        lowerEnergy += static_cast<double>(left[index]) * left[index];
        higherEnergy += static_cast<double>(chunkLeft[index]) * chunkLeft[index];
    }
    requireLattice(lowerEnergy > 0.0 && higherEnergy < .05 * lowerEnergy,
                   "Lattice: Nyquist taper is cancelled by mode normalization");

    // Pitch bend settles to +12 semitones; its fundamental stays anchored.
    synth.reset();
    synth.pitchBend(12.f);
    synth.noteOn(69, 100);
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    requireLattice(projection(left, 880.0, 48000.0) > 20.0 * projection(left, 440.0, 48000.0),
                   "Lattice: octave pitch bend failed");

    for (double rate : { 8000.0, 44100.0, 96000.0, 192000.0 }) {
        synth.prepare(rate);
        parameters.diffusion = 1.f;
        parameters.tension = 1.f;
        parameters.memory = 0.f;
        parameters.brightness = 1.f;
        parameters.outputDb = 6.f;
        parameters.attackMs = .5f;
        synth.setParameters(parameters);
        synth.reset();
        for (int note = 12; note < 120; note += 4) synth.noteOn(note, 127);
        synth.pitchBend(48.f);
        synth.render(left.data(), right.data(), static_cast<int>(left.size()));
        for (std::size_t index = 0; index < left.size(); ++index)
            requireLattice(std::isfinite(left[index]) && std::isfinite(right[index])
                               && std::abs(left[index]) <= 1.f && std::abs(right[index]) <= 1.f,
                           "Lattice: extreme setting produced non-finite or unbounded audio");
    }

    // Invalid host data must not poison subsequent rendering.
    parameters.diffusion = std::numeric_limits<float>::quiet_NaN();
    parameters.attackMs = std::numeric_limits<float>::infinity();
    synth.setParameters(parameters);
    synth.pitchBend(std::numeric_limits<float>::quiet_NaN());
    synth.render(left.data(), right.data(), static_cast<int>(left.size()));
    for (float sample : left) requireLattice(std::isfinite(sample), "Lattice: invalid control poisoned output");
}
