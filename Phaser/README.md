# Triad Phase

A vintage-inspired phaser for **Mac + Ableton Live**, with universal Apple Silicon/Intel **VST3**, **Audio Unit**, and standalone app builds. Its original DSP is inspired by the broad character of classic MXR Phase 90/Phase 45 pedals: four or two first-order all-pass stages, an exponential sweep, dry/wet cancellation, and optional feedback. **It is not an exact circuit emulation, measured hardware match, or an official MXR product.** No MXR branding or assets are included.

## Install

Extract **Triad-Phase-Mac.zip**, open the **dist** folder, and run **Install.command**. This installs the VST3/AU into your user's Library without an administrator password. Restart Ableton, enable **Use VST3 Plug-In System Folders** in Settings → Plug-Ins, and rescan. Find **Triad Phase** under **Plug-Ins → VST3 → Triad Audio**. Audio Units can also be enabled. macOS 11 or later is required. The standalone app is optional.

The bundles are ad-hoc signed development builds, not Apple-notarized distribution packages. If macOS blocks an item, inspect its security message before proceeding.

## Start with the classic sound

Choose **Classic slow sweep**: four stages, Smooth character, Rate 0.45 Hz, full Depth, 50% Mix, no Stereo Spread. Put it on guitar, keys, synths, drums, or vocals. Adjust **Rate** to taste. This starting point focuses on the simple slow swirl of old four-stage pedals.

**Resonant swirl** adds feedback for stronger colour. **Gentle two-stage** uses one cancellation notch for a subtler sound. **Fast swirl** speeds up the movement. **Wide stereo** offsets the two channel sweeps.

## Controls

- **Rate:** 0.05–10 Hz in free mode. The sweep is a sine LFO driving an exponential all-pass corner sweep from roughly 160 to 1800 Hz at full Depth.
- **Depth:** sweep range around its centre. Zero freezes the corner at about 537 Hz; it does not bypass the phaser.
- **Character:** Smooth has no feedback. Resonant enables the **Resonance** control, up to 70% feedback, with mild saturation in the feedback return. Smooth is inspired by the less resonant early pedal character; Resonant gives a stronger, later-pedal-like sweep. These are tonal references, not revision-accurate hardware models.
- **Stages:** four gives two cancellation notches at a 50/50 dry/wet mix; two gives one notch.
- **Dry / wet:** default 50%. At 100%, only the all-pass path is heard, so the familiar dry/wet cancellation notches disappear.
- **Stereo spread:** 0–180 degrees of LFO offset between left/right. Zero retains identical channel processing. It has no effect on a mono bus; stereo settings change the sound when summed to mono.
- **Output:** -18 to +12 dB trim. The phaser does not include a limiter. Resonance can increase peak level; watch the output meter.
- **Tempo sync:** derives the sweep rate from the host BPM and selected period. The period is one full LFO cycle, from a sixteenth note to four bars (bars assume four quarter notes). The LFO phase free-runs: it does not reset on play or align to song position. Without host tempo, the fallback is 120 BPM. Rate is disabled in sync mode.
- **Bypass:** smoothly returns the dry input while the LFO continues running. Double-click any knob to restore its default.

**Zero samples of added plugin latency.** All-pass filtering intentionally changes signal phase; zero reported delay does not imply phase-neutral processing. Mono and stereo layouts are supported. All parameters support DAW automation and save with the project. The LFO restarts when the audio engine is prepared, so recalling a project preserves settings but not the previous LFO phase.

## Build

Requires Xcode command line tools (`xcode-select --install`), CMake 3.22+, and Internet access to download JUCE 8.0.6. From this **Phaser** folder:

```sh
bash scripts/Build-Mac.command
```

This compiles universal arm64/x86_64 VST3/AU/standalone bundles, runs tests, renders a native editor preview, ad-hoc signs the bundles, and packages **Triad-Phase-Mac.zip**. GitHub Actions also builds that archive. Source alone is not an installable plugin. JUCE licensing must be satisfied before distributing a product built with it: https://juce.com/legal/juce-8-licence/.

## Verification

Dependency-free DSP tests:

```sh
g++ -std=c++17 -O2 -I Source tests/DSPTests.cpp -o dsp-tests
./dsp-tests
```

Tests check the exact bilinear-warped four/two-stage notch locations, wet-only all-pass amplitude, tempo-rate mapping, stereo spread, dry/bypass delay alignment, maximum-feedback stability, and host-block-size invariance at 44.1/48/96/192 kHz. Native tests check parameter recall, matching output with host BPM, zero host latency, editor construction, and preview rendering. Automated synthetic-signal tests do not establish subjective similarity to a particular pedal; Ableton listening and compatibility testing remain necessary.
