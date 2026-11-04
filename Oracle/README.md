# Triad Oracle

An original **five-voice, analogue-inspired polysynth** for Mac + Ableton Live. The architecture takes inspiration from the classic Prophet family: two oscillators, oscillator sync, a resonant four-pole filter, separate filter/amp envelopes, and cross-modulation for brass, pads, bass, keys, and leads. It is **not an exact Prophet circuit emulation or an official Sequential product**. The interface, patches, and DSP are original Triad designs.

Successful Mac builds contain universal Apple Silicon/Intel **VST3**, **Audio Unit**, and a standalone instrument app. Requires macOS 11+. The bundles are ad-hoc signed development builds, not Apple-notarized distribution releases.

## Install and hear it

1. Extract **Triad-Oracle-Mac.zip**, open **dist**, and run **Install.command**.
2. Restart Ableton. Enable **Use VST3 Plug-In System Folders** in Settings → Plug-Ins, then rescan.
3. Create a **MIDI track**, then load **Plug-Ins → VST3 → Triad Audio → Triad Oracle** onto it.
4. Arm the track and play your MIDI keyboard, play a MIDI clip, or click the instrument's on-screen keys. Check Ableton's audio output and track volume if there is no sound.

This is an instrument, not an audio effect. Sending a vocal or guitar recording into it will not generate notes. The standalone app needs an audio output selected in its audio/MIDI settings; an external MIDI keyboard is optional because the on-screen keys work too.

The factory starting sound is **Warm Brass**. The preset menu also includes Velvet Pad, Bright Brass, Sync Lead, Poly-Mod Bell, Round Bass, Glass Keys, Slow Motion, Pulse Strings, and Fifths. Some pad attacks take a few seconds to open fully.

## Synthesis

- **Five polyphonic voices** with release-aware stealing, then oldest-held-note stealing when all voices are held.
- **Two oscillators per voice:** saw, pulse, and triangle; oscillator B coarse tuning ±24 semitones and fine detune ±30 cents. The oscillator balance goes from A-only to B-only. Pulse width affects pulse waves only.
- **Oscillator sync:** oscillator A resets B. Raise B's coarse tuning and its mix level to hear stronger sync colour. This is digital hard sync, not a transistor-level model.
- **Noise mixer** and adjustable per-voice tuning offsets.
- **Four-pole low-pass:** two cascaded state-variable low-pass sections, resonance, optional key tracking, velocity influence, and bipolar filter-envelope depth. It is not a measured SSM/CEM filter replica and does not promise hardware-like self-oscillation.
- **Separate ADSR envelopes** for amplitude and filter. Times are in milliseconds; sustain is a level. Changing release time after a note starts releasing does not reshape that release already in progress.
- **Modulation:** a global sine LFO for vibrato and filter movement, plus oscillator B and the filter envelope routed to A's pitch. These are a focused set of routes rather than the complete original hardware modulation matrix.
- **Stereo:** per-voice panning and an optional stereo chorus. Zero spread plus zero chorus centres the image.
- **Alias reduction:** oscillators and filters run internally at 2× the host sample rate; saw/pulse use PolyBLEP edge corrections, with filtered downsampling. Strong hard sync and cross-modulation can still alias, particularly on high notes.
- **Output:** conservative gain staging and a soft-saturating final output. The sample amplitude remains below full scale; this is not a mastering or true-peak limiter.

## Playing and controls

The editor has three pages: **Voice & Filter**, **Envelopes**, and **Modulation & FX**. Waveform selectors, sync, key tracking, presets, Panic, and the piano keyboard remain accessible. Double-click a knob to restore its default. The right panel shows active voices and output peak level.

MIDI support: note-on/off, velocity, sustain pedal (CC64), modulation wheel (CC1, adds vibrato), pitch bend (±2 semitones), controller reset (CC121), all-notes-off (CC123), and all-sounds-off (CC120 clears voices for its channel; shared effects may retain a short tail). Sustain and bend are channel-aware. Aftertouch, MPE, MIDI program changes, arpeggiation, and sequencing are not implemented in this version.

**Panic** immediately clears all voices and chorus/filter tails. All sound controls and switches are host-automatable and saved in the project. Preset menus apply starting values; edits after selecting a preset are the saved sound. Recalls preserve patch settings, not already-sounding notes or effect buffers. The random phase generator is deterministic when the engine is prepared. CPU usage rises with polyphony, sample rate, and modulation intensity.

The synth has no look-ahead buffer or additional reported plugin latency. The interface buffer, MIDI timing, and envelope attack still affect the perceived response. The internally oversampled/downsampled signal has its normal filter phase response.

## Build on Mac

Install Xcode command line tools (`xcode-select --install`), CMake 3.22+, and use an Internet connection for JUCE 8.0.6. From this **Oracle** directory:

```sh
bash scripts/Build-Mac.command
```

This builds universal VST3/AU/standalone versions, runs tests, renders the editor preview, ad-hoc signs bundles, and creates **Triad-Oracle-Mac.zip**. GitHub Actions builds the same archive. Source code alone cannot be installed in Ableton. JUCE licensing must be satisfied before distributing a product built with it: https://juce.com/legal/juce-8-licence/.

## Tests

Dependency-free DSP checks:

```sh
g++ -std=c++17 -O2 -I Source tests/DSPTests.cpp -o dsp-tests
./dsp-tests
```

They cover note sound and tuning, pitch bend, five-voice stealing, repeated pitches, sustain/channel isolation, release, panic, extreme oscillator/filter/modulation stability, and block-size invariance at 44.1/48/96/192 kHz. Native processor tests verify sample-offset MIDI, project recall and matching audio, panic, on-screen keyboard audio, all ten factory patches producing sound, and editor construction/rendering. Synthetic tests do not establish subjective hardware similarity; Ableton compatibility and listening tests remain necessary.
