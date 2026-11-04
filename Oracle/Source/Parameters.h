#pragma once
#include <array>
struct ControlSpec { const char* id; const char* label; float min, max, initial, step, skew; const char* suffix; };
inline constexpr std::array<ControlSpec, 25> oracleControls {{
    {"mix", "OSC A / B", 0, 100, 50, 0.1f, 1, " % B"},
    {"coarse", "OSC B TUNE", -24, 24, 0, 1, 1, " st"},
    {"detune", "OSC B DETUNE", -30, 30, 7, 0.1f, 1, " ct"},
    {"pulse", "PULSE WIDTH", 15, 85, 50, 0.1f, 1, " %"},
    {"noise", "NOISE", 0, 30, 0, 0.1f, 1, " %"},
    {"cutoff", "FILTER CUTOFF", 30, 12000, 1800, 1, 0.25f, " Hz"},
    {"resonance", "RESONANCE", 0, 90, 20, 0.1f, 1, " %"},
    {"filteramount", "FILTER ENV", -4, 6, 2, 0.01f, 1, " oct"},
    {"ampattack", "AMP ATTACK", 0.5f, 5000, 10, 0.1f, 0.3f, " ms"},
    {"ampdecay", "AMP DECAY", 5, 5000, 400, 1, 0.35f, " ms"},
    {"ampsustain", "AMP SUSTAIN", 0, 100, 70, 0.1f, 1, " %"},
    {"amprelease", "AMP RELEASE", 5, 8000, 500, 1, 0.35f, " ms"},
    {"filterattack", "FILTER ATTACK", 0.5f, 5000, 10, 0.1f, 0.3f, " ms"},
    {"filterdecay", "FILTER DECAY", 5, 5000, 600, 1, 0.35f, " ms"},
    {"filtersustain", "FILTER SUSTAIN", 0, 100, 25, 0.1f, 1, " %"},
    {"filterrelease", "FILTER RELEASE", 5, 8000, 400, 1, 0.35f, " ms"},
    {"lforate", "LFO RATE", 0.05f, 20, 5, 0.01f, 0.4f, " Hz"},
    {"vibrato", "VIBRATO", 0, 100, 0, 0.1f, 1, " ct"},
    {"filterlfo", "FILTER LFO", 0, 2, 0, 0.01f, 1, " oct"},
    {"polyb", "OSC B > A PITCH", 0, 100, 0, 0.1f, 1, " %"},
    {"polyenv", "ENV > A PITCH", -100, 100, 0, 0.1f, 1, " %"},
    {"chorus", "STEREO CHORUS", 0, 40, 10, 0.1f, 1, " %"},
    {"spread", "VOICE SPREAD", 0, 100, 35, 0.1f, 1, " %"},
    {"drift", "VOICE DETUNE", 0, 100, 10, 0.1f, 1, " %"},
    {"volume", "MASTER", -24, 0, -6, 0.1f, 1, " dB"}
}};
inline constexpr std::array<const char*, 10> patchNames {"Warm Brass", "Velvet Pad", "Bright Brass", "Sync Lead", "Poly-Mod Bell", "Round Bass", "Glass Keys", "Slow Motion", "Pulse Strings", "Fifths"};
