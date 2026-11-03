#pragma once
#include <array>
namespace triadfx {
struct Parameter { const char* id; const char* label; float min, max, step, skew, initial; const char* suffix; const char* help; };
#if TRIAD_MASTERING
inline constexpr const char* productName = "Triad Master";
inline constexpr const char* stateName = "TriadMasterState";
inline constexpr std::array<Parameter, 16> parameters {{
    {"input", "INPUT", -18, 18, 0.1f, 1, 0, " dB", "Level entering the mastering chain."},
    {"hpf", "LOW CUT", 20, 120, 1, 0.5f, 25, " Hz", "Remove subsonic energy before compression."},
    {"low", "LOW SHELF", -6, 6, 0.1f, 1, 0, " dB", "Broad low shelf at 120 Hz."},
    {"midfreq", "MID FREQ", 200, 4000, 1, 0.4f, 1000, " Hz", "Centre frequency of the broad mid bell."},
    {"mid", "MID GAIN", -6, 6, 0.1f, 1, 0, " dB", "Broad bell EQ gain."},
    {"high", "AIR SHELF", -6, 6, 0.1f, 1, 0, " dB", "Broad high shelf at 9 kHz."},
    {"threshold", "THRESHOLD", -36, 0, 0.1f, 1, -18, " dB", "Compressor threshold; lower values give more compression."},
    {"ratio", "RATIO", 1, 4, 0.1f, 1, 1.5f, ":1", "Linked stereo compressor ratio with a 6 dB soft knee."},
    {"attack", "ATTACK", 1, 100, 0.1f, 0.5f, 30, " ms", "Compressor gain-reduction attack."},
    {"release", "RELEASE", 30, 600, 1, 0.5f, 150, " ms", "Compressor gain-reduction release."},
    {"makeup", "MAKEUP", 0, 12, 0.1f, 1, 0, " dB", "Makeup gain before the final limiter."},
    {"saturation", "COLOUR", 0, 100, 0.1f, 1, 0, " %", "Blend in soft saturation. Zero is clean."},
    {"width", "WIDTH", 0, 150, 0.1f, 1, 100, " %", "Mid/side width; zero makes the output mono."},
    {"monobass", "MONO BASS", 20, 200, 1, 0.6f, 100, " Hz", "Roll low bass out of the stereo side signal."},
    {"loudness", "LIMITER DRIVE", 0, 18, 0.1f, 1, 0, " dB", "Gain before the sample-peak limiter. Watch limiting reduction."},
    {"ceiling", "CEILING", -3, -0.1f, 0.1f, 1, -1, " dBFS", "Output sample-peak ceiling; this is not a true-peak limiter."}
}};
inline constexpr std::array<const char*, 4> flagIds {"eq", "compressor", "limiter", "bypass"};
inline constexpr std::array<const char*, 4> flagLabels {"EQ", "COMPRESSOR", "LIMITER", "BYPASS"};
inline constexpr std::array<bool, 4> flagDefaults {true, true, true, false};
#else
inline constexpr const char* productName = "Triad DeEss";
inline constexpr const char* stateName = "TriadDeEssState";
inline constexpr std::array<Parameter, 8> parameters {{
    {"frequency", "SIBILANCE FREQ", 3000, 11000, 1, 0.65f, 6500, " Hz", "Centre of the sibilance detector. Monitor to locate harsh S sounds."},
    {"threshold", "THRESHOLD", -48, -3, 0.1f, 1, -24, " dB", "Lower values trigger more de-essing."},
    {"ratio", "RATIO", 1, 12, 0.1f, 0.7f, 4, ":1", "Strength of gain reduction above the detector threshold."},
    {"range", "MAX REDUCTION", 0, 18, 0.1f, 1, 9, " dB", "Maximum reduction applied to the selected high band or whole signal."},
    {"attack", "ATTACK", 0.1f, 10, 0.1f, 0.5f, 0.5f, " ms", "How quickly sibilance gain reduction responds."},
    {"release", "RELEASE", 15, 250, 1, 0.6f, 80, " ms", "How quickly the vocal returns to its original level."},
    {"mix", "DRY / WET", 0, 100, 0.1f, 1, 100, " %", "Blend the processed and original signal."},
    {"output", "OUTPUT", -18, 12, 0.1f, 1, 0, " dB", "Output trim; this plugin does not include an output limiter."}
}};
inline constexpr std::array<const char*, 3> flagIds {"wide", "listen", "bypass"};
inline constexpr std::array<const char*, 3> flagLabels {"WIDEBAND", "MONITOR SIBILANCE", "BYPASS"};
inline constexpr std::array<bool, 3> flagDefaults {false, false, false};
#endif
} // namespace triadfx
