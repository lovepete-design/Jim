# Triad DeEss + Triad Master

Two native audio plugins for **macOS + Ableton Live**, with universal Apple Silicon/Intel **VST3**, **Audio Unit**, and standalone app targets. Built in C++17 with JUCE 8. These are first-version development plugins, not Apple-notarized commercial releases.

## Install

Extract the downloaded ZIP. Open its **dist** folder and run **Install.command**. The installer copies any included Triad DeEss/Master plugins into your user's Library; no administrator password is required. Restart Ableton, enable **Use VST3 Plug-In System Folders** in Settings → Plug-Ins, and click **Rescan**. Find them under **Plug-Ins → VST3 → Triad Audio**. Audio Units can also be enabled in Ableton.

The standalone apps can process a selected audio input but are not required for Ableton. The plugins need macOS 11 or later. Both Apple Silicon and Intel binaries are included in a successful universal build.

## Triad DeEss

Insert this on a vocal track. Start with **Gentle vocal**. Enable **Monitor sibilance** and adjust **Sibilance freq** until harsh S/SH sounds stand out, then turn monitoring off. Lower **Threshold** until the reduction meter moves mainly during those sounds. Adjust **Max reduction** to avoid excessive dulling.

- Default split-band mode reduces high-frequency content while preserving more of the vocal body.
- **Wideband** reduces the entire signal when the sibilance detector triggers.
- **Ratio**, **Attack**, and **Release** control reduction strength and envelope.
- **Dry / wet** blends the result. **Output** is a trim, not a limiter.
- **Monitor sibilance** auditions the detector band, independent of dry/wet or output trim. Remember to disable it before bouncing.
- The stereo detector is linked so either channel can trigger the same reduction envelope.
- **Zero added plugin latency.** Causal filtering and attack/release still affect the audio response.

Reduction is level-dependent band compression, not speech recognition. Bright instruments can trigger it too. The maximum-reduction setting caps the gain envelope; attenuation of a mixed full-band signal depends on its spectral content.

## Triad Master

Put this near the end of Ableton's master chain. The processing order is:

**Input → low cut / low shelf / mid bell / air shelf → stereo-linked soft-knee compressor → soft saturation → stereo width & mono bass → makeup / limiter drive → linked look-ahead sample-peak limiter.**

- EQ covers low cut (20–120 Hz), low shelf at 120 Hz, a variable mid bell, and high shelf at 9 kHz. Shelf and bell gains are ±6 dB.
- Compressor has threshold, ratio, attack, release, makeup, and a 6 dB soft knee.
- **Colour** blends optional soft saturation. Default is zero. The saturation stage is not oversampled; use it lightly and listen for artifacts.
- **Width** adjusts mid/side balance; zero is mono. **Mono bass** gradually rolls low frequencies out of the side channel; it is not a brick-wall crossover.
- **Limiter drive** raises level before the final limiter. **Ceiling** sets its output sample-peak bound. A -1 dBFS starting point leaves some headroom.
- **EQ**, **Compressor**, and **Limiter** switches let you bypass those stages. Limiter off can allow clipping. Plugin bypass keeps the dry signal aligned with the reported delay.
- **2 ms added latency**, reported to Ableton for delay compensation, even with the limiter disabled. Changes in sample rate reset the engines.
- Meters show input/output sample peaks, output RMS, compressor reduction, and limiter reduction. Reduction meters show recent peaks and decay visually.

This is a **sample-peak limiter, not a true-peak limiter**. It does not measure LUFS, inter-sample peaks, or streaming normalization; it has no automatic loudness target. Use a separate true-peak/LUFS meter to assess delivery requirements. Settings that sound good depend on the mix; presets are starting points, not automatic mastering. Match perceived levels when comparing processed and bypassed audio.

## Shared controls and recall

Double-click a knob to restore its default. Every audio parameter and switch is host-automatable and saved in the Ableton project. Starting-point menus apply parameter settings; the saved settings remain authoritative after further adjustment. Mono and stereo layouts are supported. Bypass and continuous controls fade smoothly to reduce clicks. There are no audio-thread allocations, file operations, or network calls in processing.

## Build on Mac

Install Xcode command line tools (`xcode-select --install`) and CMake 3.22+, then from this **Effects** folder run:

```sh
bash scripts/Build-Mac.command
```

The script downloads JUCE 8.0.6, builds both universal plugins and standalone apps, runs DSP and native-processor tests, ad-hoc signs the bundles, and produces `Triad-Effects-Mac.zip`. The GitHub Actions workflow builds downloadable per-plugin ZIPs on macOS. Source code alone cannot be installed as a VST.

JUCE licensing must be satisfied before distributing a product built with it: https://juce.com/legal/juce-8-licence/.

## Tests

The dependency-free DSP tests run with:

```sh
g++ -std=c++17 -O2 -I Source tests/DSPTests.cpp -o dsp-tests
./dsp-tests
```

Or use CMake with `-DTRIAD_DSP_ONLY=ON`. Tests check band selectivity, sibilance reduction/range, stereo linkage, delay alignment, mastering compression, limiter ceilings under high gain and automation, mono width, and isolated impulse timing at 44.1/48/96/192 kHz. Native processor tests additionally check parameter recall, matching audio after recall, host latency, and editor construction. Automated tests use synthetic signals; listening and compatibility testing inside Ableton remain necessary.
