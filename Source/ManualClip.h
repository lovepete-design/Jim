#pragma once
#include "PitchEngine.h"
#include <atomic>
#include <thread>

namespace triad {
// One transferred passage, fixed-capacity storage. Ownership moves between the
// audio thread and analysis worker through the release/acquire state machine.
class ManualClip {
public:
    struct SavedNote { int start, end; float original; int transpose; bool tuned; };
    struct Snapshot {
        double rate = 48000, origin = 0;
        std::array<std::vector<float>, 2> audio;
        std::vector<SavedNote> notes;
    };
    enum State { Empty, Armed, Recording, StopRequested, AnalysisPending, Analysing, Ready };
    struct Note {
        int start = 0, end = 0;
        float original = 60;
        std::atomic<int> transpose {0};
        std::atomic<bool> tuned {false};
    };
    static constexpr int maxNotes = 1024;
    ~ManualClip() { if (worker.joinable()) worker.join(); }
    void prepare(double rate) {
        auto saved = snapshot();
        if (worker.joinable()) worker.join();
        sampleRate = rate;
        for (auto& ch : audio) ch.assign(static_cast<size_t>(rate * 120), 0);
        length = 0; noteCount = 0; state.store(Empty);
        if (!saved.audio[0].empty()) restore(saved);
    }
    // The processor holds its host callback lock during session save/restore.
    Snapshot snapshot() const {
        Snapshot result; result.rate = sampleRate; result.origin = origin;
        if (!ready()) return result;
        for (int ch = 0; ch < 2; ++ch) result.audio[ch].assign(audio[ch].begin(), audio[ch].begin() + length);
        for (int i = 0; i < noteCount; ++i) {
            const auto& n = notes[static_cast<size_t>(i)];
            result.notes.push_back({n.start, n.end, n.original, n.transpose.load(), n.tuned.load()});
        }
        return result;
    }
    void restore(const Snapshot& saved) {
        if (worker.joinable()) worker.join();
        state.store(Empty);
        if (saved.rate < 8000 || saved.audio[0].empty() || saved.audio[0].size() != saved.audio[1].size()) return;
        if (audio[0].empty()) for (auto& channel : audio) channel.resize(static_cast<size_t>(sampleRate * 120));
        const double ratio = sampleRate / saved.rate;
        length = std::min(static_cast<int>(audio[0].size()), static_cast<int>(saved.audio[0].size() * ratio));
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < length; ++i) {
                const double index = i / ratio;
                const auto first = std::min(static_cast<size_t>(index), saved.audio[ch].size() - 1);
                const auto second = std::min(first + 1, saved.audio[ch].size() - 1);
                audio[ch][static_cast<size_t>(i)] = static_cast<float>(saved.audio[ch][first]
                    + (index - first) * (saved.audio[ch][second] - saved.audio[ch][first]));
            }
        }
        origin = saved.origin; noteCount = 0;
        for (const auto& savedNote : saved.notes) {
            if (noteCount >= maxNotes) break;
            auto& n = notes[static_cast<size_t>(noteCount++)];
            n.start = std::clamp(static_cast<int>(savedNote.start * ratio), 0, length);
            n.end = std::clamp(static_cast<int>(savedNote.end * ratio), n.start, length);
            n.original = savedNote.original; n.transpose.store(std::clamp(savedNote.transpose, -12, 12));
            n.tuned.store(savedNote.tuned);
        }
        state.store(Ready, std::memory_order_release);
    }
    bool arm() {
        const auto s = state.load();
        if (s != Empty && s != Ready) return false;
        if (worker.joinable()) worker.join();
        // UI stops reading old notes once it publishes Armed.
        state.store(Armed, std::memory_order_release); return true;
    }
    void stop() {
        int expected = Recording;
        if (!state.compare_exchange_strong(expected, StopRequested)) {
            expected = Armed; state.compare_exchange_strong(expected, Empty);
        }
    }
    // Called from the audio callback before it modifies the incoming samples.
    void capture(float* const* input, int channels, int count, double hostSeconds, bool playing) {
        auto s = state.load(std::memory_order_acquire);
        if (s == Armed && playing) {
            length = 0; noteCount = 0; origin = hostSeconds;
            state.store(Recording, std::memory_order_release); s = Recording;
        }
        if (s == StopRequested || (s == Recording && !playing)) {
            state.store(AnalysisPending, std::memory_order_release); return;
        }
        if (s != Recording) return;
        // A seek/loop ends the transfer rather than splicing unrelated positions.
        if (std::abs(hostSeconds - (origin + length / sampleRate)) > 0.05) {
            state.store(AnalysisPending, std::memory_order_release); return;
        }
        const int amount = std::min(count, static_cast<int>(audio[0].size()) - length);
        for (int ch = 0; ch < 2; ++ch)
            std::copy_n(input[std::min(ch, channels - 1)], amount, audio[ch].data() + length);
        length += amount;
        if (length >= static_cast<int>(audio[0].size())) state.store(AnalysisPending, std::memory_order_release);
    }
    // Message-thread polling launches expensive analysis outside the callback.
    void service() {
        int expected = AnalysisPending;
        if (!state.compare_exchange_strong(expected, Analysing)) return;
        if (worker.joinable()) worker.join();
        worker = std::thread([this] { analyse(); state.store(Ready, std::memory_order_release); });
    }
    bool ready() const { return state.load(std::memory_order_acquire) == Ready; }
    double duration() const { return ready() ? length / sampleRate : 0; }
    double startTime() const { return ready() ? origin : 0; }
    int countNotes() const { return ready() ? noteCount : 0; }
    const Note& note(int i) const { return notes[static_cast<size_t>(i)]; }
    void transpose(int i, int semitones) {
        if (ready() && i >= 0 && i < noteCount) {
            notes[static_cast<size_t>(i)].transpose.store(std::clamp(semitones, -12, 12));
            notes[static_cast<size_t>(i)].tuned.store(true);
        }
    }
    void reset(int i) {
        if (ready() && i >= 0 && i < noteCount) {
            notes[static_cast<size_t>(i)].transpose.store(0);
            notes[static_cast<size_t>(i)].tuned.store(false);
        }
    }
    double rate() const { return sampleRate; }
    // Offline source look-ahead: reads from already captured audio, so there is
    // no delay added to the host timeline. This is a prototype granular shifter.
    void playback(float* const* output, int channels, int count, double hostSeconds) const {
        if (!ready()) return;
        const int start = static_cast<int>(std::llround((hostSeconds - origin) * sampleRate));
        int noteIndex = 0;
        while (noteIndex < noteCount && notes[noteIndex].end <= start) ++noteIndex;
        for (int i = 0; i < count; ++i) {
            const int position = start + i;
            if (position < 0 || position >= length) continue; // live input outside clip
            while (noteIndex < noteCount && notes[noteIndex].end <= position) ++noteIndex;
            const Note* n = noteIndex < noteCount && notes[noteIndex].start <= position ? &notes[noteIndex] : nullptr;
            const double shift = n && n->tuned.load() ? n->transpose.load(std::memory_order_relaxed) + std::round(n->original) - n->original : 0;
            const double ratio = std::pow(2.0, shift / 12.0);
            const double frequency = n ? 440 * std::pow(2.0, (n->original - 69) / 12.0) : 220;
            // Align overlapping grains to an even number of source periods.
            const double window = sampleRate / frequency * 2 * std::max(1.0, std::round(0.04 * frequency / 2));
            const double phase = n ? (position - n->start) * (1 - ratio) / window : 0;
            const double p = phase - std::floor(phase), q = p < 0.5 ? p + 0.5 : p - 0.5;
            const double weight = 0.5 - 0.5 * std::cos(2 * pi * p);
            // Short ramps blend each note boundary back to its original audio.
            const double blend = n ? std::clamp(std::min(position - n->start, n->end - position) / (sampleRate * 0.005), 0.0, 1.0) : 0;
            for (int ch = 0; ch < channels; ++ch) {
                const double dry = audio[static_cast<size_t>(ch)][static_cast<size_t>(position)];
                const double wet = shift == 0 ? dry : weight * read(ch, position - (p - 0.5) * window)
                    + (1 - weight) * read(ch, position - (q - 0.5) * window);
                output[ch][i] = static_cast<float>(dry + blend * (wet - dry));
            }
        }
    }
    std::atomic<int> state {Empty};
private:
    std::array<std::vector<float>, 2> audio;
    std::array<Note, maxNotes> notes;
    std::thread worker;
    double sampleRate = 48000, origin = 0;
    int length = 0, noteCount = 0;
    double read(int ch, double position) const {
        if (position < 0 || position >= length - 1) return 0;
        const auto first = static_cast<size_t>(position);
        const auto& line = audio[static_cast<size_t>(ch)];
        return line[first] + (position - first) * (line[first + 1] - line[first]);
    }
    void analyse() {
        Detector detector;
        const int decimation = std::max(1, static_cast<int>(sampleRate / 12000));
        detector.prepare(sampleRate / decimation);
        const double coefficient = 1 - std::exp(-2 * pi * 1600 / sampleRate);
        double low1 = 0, low2 = 0;
        int active = -1, lastVoiced = 0;
        double pitchSum = 0; int pitchCount = 0;
        auto finish = [&] {
            if (active < 0) return;
            auto& n = notes[static_cast<size_t>(active)];
            n.end = lastVoiced;
            if (n.end - n.start >= static_cast<int>(sampleRate * 0.06)) {
                n.original = static_cast<float>(pitchSum / std::max(1, pitchCount)); ++noteCount;
            }
            active = -1; pitchSum = 0; pitchCount = 0;
        };
        // YIN frames lag by half the 1024-sample detector window. Timestamp the
        // detected pitch at its centre rather than at the end of analysis.
        for (int i = 0; i < length; ++i) {
            low1 += coefficient * (audio[0][static_cast<size_t>(i)] - low1);
            low2 += coefficient * (low1 - low2);
            if (i % decimation != 0 || !detector.push(static_cast<float>(low2))) continue;
            const int position = std::max(0, i - 512 * decimation);
            const bool voiced = detector.frequency > 0 && detector.confidence > 0.85;
            if (!voiced) { if (position - lastVoiced > sampleRate * 0.06) finish(); continue; }
            const double midi = 69 + 12 * std::log2(detector.frequency / 440);
            if (active >= 0 && std::abs(midi - notes[static_cast<size_t>(active)].original) > 0.75) finish();
            if (active < 0) {
                if (noteCount >= maxNotes) break;
                active = noteCount;
                auto& n = notes[static_cast<size_t>(active)];
                n.start = std::max(0, position - 64 * decimation);
                n.original = static_cast<float>(midi); n.transpose.store(0); n.tuned.store(false);
            }
            lastVoiced = std::min(length, position + 64 * decimation);
            pitchSum += midi; ++pitchCount;
        }
        finish();
    }
};
} // namespace triad
