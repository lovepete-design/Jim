#pragma once
#include <array>
#include <cstddef>
#include <memory>

namespace triad {
struct HexParameters {
    float outputDb = -12.0f;
    float tuneCents = 0.0f;
    int algorithm = 0; // Zero based, 0..31.
    int feedback = 0;  // 0..7.
};

// An allocation-free render path around Dexed's MSFA/MkI DX7 core.
// MIDI and parameter methods must run on the same thread as render().
class HexEngine {
public:
    HexEngine();
    ~HexEngine();
    HexEngine(const HexEngine&) = delete;
    HexEngine& operator=(const HexEngine&) = delete;
    void prepare(double sampleRate);
    void reset();
    void setParameters(const HexParameters&);
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void sustain(bool down);
    void pitchBend(float semitones);
    void render(float* left, float* right, int count) noexcept;
    bool loadSysex(const unsigned char* data, std::size_t size);
    static bool parseSysex(const unsigned char* data, std::size_t size,
                          std::array<unsigned char,155>& result) noexcept;
    const std::array<unsigned char,155>& patch() const;
    void setPatch(const std::array<unsigned char,155>&);
    int latencySamples() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
