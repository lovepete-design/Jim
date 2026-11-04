# Aurora 224 — Triad Audio

An original stereo algorithmic reverb inspired by the lush, dark, modulated character of the Lexicon 224. This is not a recreation of its firmware, original algorithms, converters, or an authenticated hardware model. No Lexicon code or assets are used. Lexicon is a trademark of its respective owner; this project is independent.

## Mac

See `MAC-START-HERE.md` and run `Build-and-Install-Mac.command` on your Mac to compile and install a universal Apple Silicon/Intel VST3. This launcher has not been executed on macOS here.

## Included build

`dist/Aurora224-linux-x86_64.zip` contains the compiled **Linux x86-64 VST3** bundle. It cannot run on macOS or Windows. VST3 uses `.vst3`; renaming it to the legacy `.vst` extension will not convert it.

Extract and copy the entire `Aurora224.vst3` directory into `~/.vst3/`, then rescan plugins in your DAW. Insert **Aurora 224** on a stereo track or stereo effects return. Use the DAW's generic parameter/controls view: this release has no custom graphical editor. Mono buses and 64-bit sample buffers are not supported; 32-bit floating-point stereo processing is supported. A DAW's 64-bit executable is compatible with 32-bit sample processing.

## Controls

| Control | Range / behavior |
| --- | --- |
| Algorithm | Concert Hall, Plate, Chamber; changes delay geometry |
| Decay | 0.3–12 seconds, nominal midband RT60 target |
| Predelay | 0–250 ms |
| Size | Scales tank and diffuser delay lengths |
| Diffusion | Input all-pass feedback/density |
| Damping | Higher values darken the tail (18 kHz down to 1.2 kHz) |
| Bass Decay | Low-frequency decay from 0.5× to 1.5× nominal decay |
| Modulation | Fractional tank delay modulation, up to approximately 0.65 ms |
| Width | Mono wet field at 0%, full stereo at 100% |
| Mix | Linear dry/wet; 100% for an effects return |
| Bypass | Dry passthrough; reverb continues processing internally |

Continuous controls use 40 ms smoothing. Algorithm changes and bypass are discrete; switch these between passages for clean transitions. Size and predelay automation can produce pitch movement. Parameters and bypass persist with the DAW project. The reported tail is 22 seconds to accommodate the longest bass decay; rendered decay times depend on damping, size, diffusion, and modulation.

Suggested starting points (enter through the host controls):

| Sound | Mode | Decay | Predelay | Damping | Modulation | Mix |
| --- | --- | --- | --- | --- | --- | --- |
| Vocal Hall | Concert Hall | 3.1 s | 35 ms | 65% | 30% | 18% |
| Wide Synth | Concert Hall | 7.5 s | 20 ms | 55% | 65% | 35% |
| Snare Plate | Plate | 1.8 s | 12 ms | 35% | 20% | 20% |
| Small Chamber | Chamber | 0.9 s | 5 ms | 60% | 15% | 15% |

## Build on macOS, Windows, or Linux

Requirements: Git, CMake 3.25+, and a C++17 compiler. On macOS install Xcode Command Line Tools; on Windows use Visual Studio 2022 with Desktop development with C++. No JUCE, GUI framework, or audio-device library is needed.

Clone the official MIT-licensed VST3 SDK into a directory of your choice and pin the revision tested here:

```sh
git clone --recursive https://github.com/steinbergmedia/vst3sdk.git vst3sdk
git -C vst3sdk checkout 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
git -C vst3sdk submodule update --init --recursive
cmake -S . -B build -DVST3_SDK_ROOT=/absolute/path/to/vst3sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Run CMake from this project directory. On Windows replace the SDK path with a path such as `C:/dev/vst3sdk`. On macOS add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure command for a universal bundle. The SDK runs its validator after linking. Output: `build/VST3/Release/Aurora224.vst3` (for multi-configuration generators, inspect `build/VST3` if layout differs).

Install the whole bundle:

- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `C:\Program Files\Common Files\VST3\`
- Linux: `~/.vst3/`

macOS and Windows builds have not been compiled or tested in this Linux environment. Local unsigned macOS builds may require the usual local signing steps for your system and DAW. This project does not include an Audio Unit version for Logic Pro.

## Architecture and verification

Four input all-pass diffusers feed an eight-line fractional-delay feedback network. An orthogonal Householder matrix recirculates energy; independent phase-shifted modulation decorrelates the tail. Each line has low- and high-frequency decay shaping. Delay memory is allocated during activation; processing performs no heap allocation or file I/O. Stereo input/output supports in-place buffers, silent input tails, sample-offset parameter changes, and versioned little-endian state.

The DSP test covers Hall/Plate/Chamber at 44.1/48/96 kHz, finite and bounded impulse responses, stereo decorrelation, decay, exact dry mix, and extreme settings with size automation. The official VST3 validator passed 47 plugin tests on Linux. This verifies basic DSP and host contracts, not perceptual equivalence to a Lexicon 224 or compatibility with every DAW. Listening against hardware and real-DAW testing remain necessary before production use.

Source is MIT licensed. See `LICENSE` and `docs/VST3-SDK-LICENSE.txt` for SDK attribution.
