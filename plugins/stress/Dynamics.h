#pragma once

#include <array>
#include <atomic>

namespace triad {

struct DynamicsParameters {
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    float attackMs = 5.0f;
    float releaseMs = 100.0f;
    float ratio = 4.0f;
    float mix = 1.0f;
    // 0 = clean, 1 = second harmonic, 2 = third harmonic.
    int distortion = 0;
    bool detectorHighpass = false;
    bool emphasis = false;
};

// Independent, zero-reported-latency, stereo-linked dynamics model.
// Call prepare/reset/setParameters on the audio thread, or while stopped.
// process supports identical left/right pointers for mono processing.
class Dynamics {
public:
    void prepare(double sampleRate);
    void reset();
    void setParameters(const DynamicsParameters& parameters);
    void process(float* left, float* right, int count) noexcept;
    float gainReductionDb() const noexcept;

private:
    struct Channel {
        double hpX = 0.0, hpY = 0.0, emphasisLow = 0.0;
        double previousWet = 0.0, previousPrimitiveOdd = 0.0, previousPrimitiveEven = 0.0;
        double oddDcX = 0.0, oddDcY = 0.0;
        double evenDcX = 0.0, evenDcY = 0.0;
        bool primitiveValid = false;
    };

    DynamicsParameters parameters_ {};
    std::array<Channel, 2> channels_ {};
    double sampleRate_ = 44100.0;
    double smooth_ = 0.0015, hpPole_ = 0.99, emphasisPole_ = 0.7, dcPole_ = 0.999;
    double inputDb_ = 0.0, outputDb_ = 0.0, attackMs_ = 5.0, releaseMs_ = 100.0;
    double slope_ = 0.75, mix_ = 1.0, h2_ = 0.0, h3_ = 0.0;
    double hpMix_ = 0.0, emphasisMix_ = 0.0;
    double detectorPower_ = 0.0, reduction_ = 0.0, releaseMemory_ = 0.0;
    bool initialised_ = false;
    std::atomic<float> meter_ {0.0f};
};

} // namespace triad
