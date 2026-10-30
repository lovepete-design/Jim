# Triad Audio

Three native C++ VST3 plugins with custom interfaces, automation, project-state
recall, and authored starting presets. The primary build target is macOS,
universal Apple Silicon + Intel. Version 0.1.0 is an initial development release.

| Plugin | Purpose | Implementation |
| --- | --- | --- |
| **Hex MkI** | Yamaha DX7 emulation | Dexed MkI/MSFA fixed-point engine; six operators, all 32 algorithms, 16 voices, DX7 envelopes, keyboard/velocity scaling, LFO, and validated voice/bank SysEx import. |
| **Stress** | Distressor-style compression | Independent stereo-linked compressor with eight ratios, program-dependent release, detector filters, even/odd harmonic modes, parallel mix, and gain-reduction meter. |
| **Lattice** | Experimental synthesis | Twelve-voice network of phase-coupled modes; spectral energy diffuses between neighbors and reacts to their phase gradients. |

**macOS binaries have not been built in this Linux workspace.** Build the three
Mac `.vst3` bundles with the included script, or use the macOS CI workflow. Linux
build/test results are recorded in `docs/VALIDATION.md`. Linux VST3 bundles cannot
be installed on macOS.

## Build on macOS

After installing the prerequisites below, double-click
`Build-and-Install-Mac.command` to build and install all three plugins. This
launcher limits compilation to three jobs and also finds the CMake application
in `/Applications`.

Install Apple's command line tools (`xcode-select --install`) and CMake
(`brew install cmake`), then run from this directory:

```sh
bash scripts/build-macos.sh
```

The script builds universal plugins, runs the DSP and VST3 host tests, applies
local ad hoc signatures, and creates `dist/triad-audio-macos.zip`. Install with:

```sh
bash scripts/install-macos.sh
```

The installer copies the bundles into `~/Library/Audio/Plug-Ins/VST3` and backs
up existing versions. It runs only when explicitly invoked. The plugins need a
VST3 host; Logic Pro requires Audio Units, which this project does not build.
See [macOS build details](docs/MACOS.md) for architecture overrides, CI, and signing.

## Use

**Hex MkI:** insert as an instrument and play MIDI. Click **Import SysEx** to load
a 163-byte single voice or 4,104-byte bank; a bank imports its first voice. Use an
external DX7 editor to edit all operator parameters, then import its voice dump.
The plugin exposes output, fine tune, algorithm, and feedback directly. Sustain
and a two-semitone pitch-bend range are supported. No factory ROM patches are
bundled. [Engine and import details](docs/HEX.md).

**Stress:** insert on a stereo audio track. Input gain drives a fixed -18 dBFS
compression threshold. Output compensates level. Choose a ratio and set attack,
release, detector filtering, harmonic mode, and dry/wet mix.
[Model details](docs/STRESS.md).

**Lattice:** insert as an instrument and play MIDI. Diffusion redistributes
spectral energy; Tension changes inharmonicity and coupling; Memory slows its
evolution; Brightness shapes excitation. Try sustained chords and automate these
four controls. [Synthesis equations and presets](docs/LATTICE.md).

## Build and test on Linux

With CMake 3.22+, a C++17 compiler, and the usual JUCE Linux development
dependencies (ALSA, X11/Xext/Xinerama/Xrandr/Xcursor, fontconfig, freetype, GL):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Host/editor tests need an X display; use `xvfb-run -a ctest --test-dir build
--output-on-failure` on a headless machine. DSP-only checks need no JUCE/system
GUI dependencies:

```sh
cmake -S . -B build-dsp -DTRIAD_BUILD_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --parallel 4
ctest --test-dir build-dsp --output-on-failure
mkdir -p previews
build-dsp/triad_tests --render previews
```

JUCE 8.0.6 is included in the source package; CMake fetches the same pinned
revision if it is absent.

## Fidelity, novelty, and licensing

The DX7 implementation uses an established emulation core; this project has
not measured it against physical DX7 hardware. Original DAC/reconstruction
behavior and several performance controllers are outside the adapter's scope.
Stress is an independent model inspired by Distressor workflows, without
measurement-based hardware matching or manufacturer endorsement. Lattice is
an original implementation for this project; no claim of historical or patent
novelty is made. Yamaha and Distressor names identify reference products only.

Original code is provided under **AGPL-3.0-or-later**. The distributed combined
plugins use JUCE's open-source AGPL terms; closed-source commercial distribution
requires reviewing JUCE and upstream licensing. The Dexed MkI code retains
GPL-3.0-or-later, and MSFA retains Apache-2.0. Full corresponding source and
notices accompany binary packages. See `LICENSE`, `THIRD_PARTY.md`, and
`THIRD_PARTY_DX7.md`.
