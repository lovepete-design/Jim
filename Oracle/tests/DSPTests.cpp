#include "SynthDSP.h"
#include <cstdlib>
#include <iostream>
using namespace oracle;
void check(bool ok, const char* message) { if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
double energy(const std::vector<float>& v) { double sum = 0; for (auto x : v) sum += x * x; return sum / std::max<size_t>(1, v.size()); }
void render(Engine& e, std::vector<float>& l, std::vector<float>& r, int block = 127) {
    for (size_t offset = 0; offset < l.size(); offset += static_cast<size_t>(block))
        e.render(l.data() + offset, r.data() + offset, static_cast<int>(std::min(static_cast<size_t>(block), l.size() - offset)));
}
double frequency(const std::vector<float>& v, double rate) {
    int count = 0; size_t first = 0, last = 0;
    for (size_t i = v.size() / 2 + 1; i < v.size(); ++i) if (v[i - 1] < 0 && v[i] >= 0) { if (count == 0) first = i; last = i; ++count; }
    return count > 1 ? (count - 1) * rate / (last - first) : 0;
}
int main() {
    for (double rate : {44100., 48000., 96000., 192000.}) {
        Engine e; e.prepare(rate); Settings s; s.chorus = 0; s.drift = 0; s.spread = 0; s.filterAmount = 0; s.keytrack = false;
        s.cutoff = 12000; s.ampAttack = 1; s.ampSustain = 1; s.mix = 0; s.waveA = 2; s.ampRelease = 50;
        e.setSettings(s);
        std::vector<float> left(static_cast<size_t>(rate), 1), right(left.size(), 1);
        render(e, left, right); check(energy(left) == 0, "Silence before MIDI notes");
        e.noteOn(1, 69, 1); render(e, left, right);
        check(energy(left) > 1e-4, "Note-on produces audible output");
        check(std::abs(frequency(left, rate) - 440) < 1, "A4 tuning is correct");
        check(left == right, "Zero spread and chorus keep centre image");
        e.pitchBend(1, 16383); render(e, left, right);
        check(std::abs(frequency(left, rate) - 493.88) < 1.5, "Pitch bend reaches approximately two semitones");
        e.control(1, 64, 127); e.noteOff(1, 69); render(e, left, right);
        check(e.activeVoices() == 1 && energy(left) > 1e-4, "Sustain holds released note");
        e.control(1, 64, 0); render(e, left, right);
        check(e.activeVoices() == 0, "Sustain release frees voice");
        check(energy(std::vector<float>(left.begin() + static_cast<long>(left.size() / 2), left.end())) < 1e-12, "Release reaches silence");
        for (int note = 60; note < 66; ++note) e.noteOn(1, note, 0.8);
        check(e.activeVoices() == 5 && e.heldVoices() == 5 && !e.hasNote(1, 60) && e.hasNote(1, 65), "Sixth note steals oldest of five voices");
        e.noteOff(1, 60); check(e.heldVoices() == 5, "Stolen note-off cannot kill replacement voice");
        e.panic(); render(e, left, right); check(energy(left) == 0 && e.activeVoices() == 0, "Panic removes voices and effect tails");
        e.noteOn(1, 60, 0.8); e.noteOn(2, 60, 0.8); e.control(1, 64, 127); e.noteOff(2, 60); render(e, left, right);
        check(e.activeVoices() == 1 && e.hasNote(1, 60), "MIDI-channel sustain is isolated");
        e.panic(); e.noteOn(1, 60, 1); e.noteOn(1, 60, 1); e.noteOff(1, 60); check(e.heldVoices() == 1, "Repeated pitches release one held voice at a time");
        e.noteOff(1, 60); check(e.heldVoices() == 0, "Second note-off releases remaining repeated note");
        for (int wave = 0; wave < 3; ++wave) {
            e.panic(); s.waveA = wave; s.waveB = wave; s.chorus = 0.4; s.spread = 1; s.resonance = 0.9;
            s.sync = true; s.polyB = 1; s.polyEnv = 1; s.filterLfo = 2; s.filterAmount = 6; s.noise = 0.3;
            e.setSettings(s); for (int n : {24, 48, 72, 96, 120}) e.noteOn(1, n, 1);
            render(e, left, right);
            for (size_t i = 0; i < left.size(); ++i)
                check(std::isfinite(left[i]) && std::isfinite(right[i]) && std::abs(left[i]) <= 1 && std::abs(right[i]) <= 1, "Extreme sync/modulation/resonance stays finite and bounded");
        }
    }
    Engine a, b; a.prepare(48000); b.prepare(48000); Settings s;
    a.setSettings(s); b.setSettings(s); a.noteOn(1, 60, 0.8); b.noteOn(1, 60, 0.8);
    std::vector<float> l1(48000), r1(48000), l2(48000), r2(48000);
    render(a, l1, r1, 1); render(b, l2, r2, 511); check(l1 == l2 && r1 == r2, "Host block size does not change the sound");
    std::cout << "PASS: note sound/tuning, pitch bend, five-voice stealing, sustain/channel isolation, repeated notes, release, panic, waveform/modulation stability, and block-size invariance at four sample rates\n";
}
