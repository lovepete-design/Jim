# Triad Tune

A native vocal pitch-correction prototype for **macOS + Ableton Live**, built with C++17 and JUCE 8. Targets universal Apple Silicon/Intel **VST3**, **Audio Unit**, and a standalone live-tuning app. Mac bundles need to be compiled on a Mac; source code alone cannot be installed in Ableton.

## Individual note editing

1. Insert Triad Tune on an audio track containing an isolated vocal. Select **Note editor** (the default).
2. Position Ableton at the beginning of the passage. Click **Transfer**, then play the passage in Ableton.
3. Click **Finish**, or stop Ableton. Wait for note detection to finish.
4. Drag a note vertically to select its target semitone (up to one octave each way). Dragging also centres that note's pitch on the target. Double-click restores the original note.
5. Replay the same timeline passage in Ableton to hear edits. Wheel to zoom; Shift + wheel to pan. The initial view shows 12 seconds, with notes C2 through C6.

The plugin replaces the captured time range with its edited recording. Audio outside that range passes through. Changes to the original clip after transfer require a new transfer. Moving the original vocal to another timeline location also requires a new transfer. Transfer is limited to **120 seconds and 1024 detected notes per instance**. A seek or loop ends a transfer.

Captured audio, detected notes, and pitch edits are embedded in Ableton's saved plugin state. **Finish analysis before saving.** Long captures increase project size (up to about 184 MB at 192 kHz stereo), and state save/restore is not intended during performance. Sample-rate changes resample the stored capture. Use a separate plugin instance for another passage.

This version edits pitch only. It does not provide Melodyne's timing tools, manual note splitting/merging, formant editing, vibrato editing, ARA integration, or polyphonic separation. Detection needs clean monophonic vocals; automatic segmentation may join repeated notes or split expressive singing. The granular shifter can produce audible texture, especially on large shifts. It is a working prototype, not a replacement for the full commercial products.

## Latency

**Note editor: zero samples of added plugin delay.** The captured source allows look-ahead reads without moving playback later on the DAW timeline. This is a transferred-audio workflow, not real-time correction of a new performance. Your audio interface and Ableton still have their own monitoring latency.

**Live tune: about 21 ms nominal added signal delay**, reported to the host. Pitch detection and the retune setting also affect how quickly correction responds to a new note; they are separate from the reported delay. Live zero-latency pitch correction is not promised. To record with minimal monitoring latency, monitor the dry input and edit the recording afterward.

## Automatic live correction

Choose **Live tune**. Select key and scale, then adjust:

- **Retune speed:** 0–200 ms. Zero gives the strongest stepped effect.
- **Correction:** how much pitch error to correct.
- **Tolerance:** cents of pitch variation to leave alone.
- **Dry / wet** and **Output:** blend and gain.

Starting points: Natural vocal, Tight pop, Hard tune. Live controls apply to Live tune only; manual notes control editor playback. Detection range is approximately 65–1000 Hz. Mono and stereo buses are supported; stereo detection uses the first channel, with linked correction for both channels. Output is not limited, so watch Ableton's meters. Bypass in Live tune keeps the delayed dry path for host alignment.

The standalone app supports live tuning; note transfer needs a DAW transport.

## Build and install on Mac

Requires macOS 11+, Xcode command line tools (`xcode-select --install`), CMake 3.22+, and Internet access for JUCE. In Terminal, from this folder:

```sh
bash scripts/Build-Mac.command
```

This builds universal arm64/x86_64 VST3/AU/app bundles, runs DSP tests, ad-hoc signs the bundles, and creates `Triad-Tune-Mac.zip`. Run `dist/Install.command` to copy plugins into your user's Library. Restart Ableton, enable **Use VST3 Plug-In System Folders** in Settings → Plug-Ins, and rescan. For Audio Units, enable Audio Units. These are development builds, not Apple-notarized distribution packages. JUCE licensing must be satisfied before distributing a product built with it (https://juce.com/legal/juce-8-licence/).

The GitHub Actions workflow also builds and packages the Mac plugins on a macOS runner for pull requests, pushes to main, or manual runs. Its downloadable artifact is `Triad-Tune-Mac-universal`. The workflow must run successfully before a Mac binary is available.

## Validate the DSP without GUI dependencies

```sh
cmake -S . -B build-core -DTRIAD_ENGINE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Tests cover key/scale quantisation, pitch detection, silence/noise rejection, live correction at 44.1/48/96 kHz, stereo linkage, dry-path delay, note transfer, individual pitch changes, exact unedited timeline alignment, random-access playback, and capture/state recall. They use synthetic signals and do not establish subjective vocal quality or Mac/Ableton compatibility; a Mac host listening test remains necessary.
