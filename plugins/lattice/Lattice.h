#pragma once

#include <array>
#include <cstdint>

namespace triad {

struct LatticeParameters {
    float diffusion = .35f;
    float tension = .25f;
    float memory = .5f;
    float brightness = .5f;
    float attackMs = 20.f;
    float releaseMs = 700.f;
    float outputDb = -9.f;
};

// A pitched oscillator network: phase gradients move energy between modes,
// and that moving energy changes the phase coupling. All storage is fixed.
class Lattice {
public:
    void prepare(double sampleRate);
    void reset();
    void setParameters(const LatticeParameters&);
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void sustain(bool pedalDown);
    void pitchBend(float semitones);
    void render(float* left, float* right, int count) noexcept;

private:
    static constexpr int modeCount = 12;
    static constexpr int voiceCount = 12;
    static constexpr int controlPeriod = 16;

    struct Mode {
        double x = 1.0, y = 0.0;
        double cosine = 1.0, sine = 0.0;
        float energy = 0.f;
        float target = 0.f;
        float amplitude = 0.f;
        float panLeft = .70710678f, panRight = .70710678f;
    };
    struct Voice {
        std::array<Mode, modeCount> modes {};
        int note = 60;
        float velocity = 0.f;
        float envelope = 0.f;
        bool active = false;
        bool held = false;
        bool sustained = false;
        std::uint64_t age = 0;
        int controlCountdown = 0;
    };

    void updateVoice(Voice&) noexcept;
    void smoothParameters() noexcept;
    double sampleRate_ = 48000.0;
    double parameterPole_ = 0.0;
    float attackPole_ = .99f;
    float releasePole_ = .999f;
    float targetBend_ = 0.f;
    float bend_ = 0.f;
    float gain_ = .3548134f;
    bool sustain_ = false;
    int parameterCountdown_ = 0;
    std::uint64_t age_ = 0;
    LatticeParameters target_ {};
    LatticeParameters smooth_ {};
    std::array<Voice, voiceCount> voices_ {};
};

} // namespace triad
