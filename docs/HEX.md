# HEX — six-operator FM

HEX uses the established Dexed MSFA note engine and MkI logarithmic-sine core.
It provides all 32 DX7 algorithms, operator feedback, four-stage rate/level
amplitude and pitch envelopes, ratio and fixed-frequency operators, keyboard
level/rate scaling, velocity sensitivity, detune, transpose, oscillator sync, and
the six-waveform LFO encoded in an imported DX7 voice. The output is authentic
mono, delivered to both output channels. Sixteen voices support MIDI note
on/off, sustain (CC64), voice stealing, and continuous pitch bend.

The original `GLASS KEYS` default voice has a bright FM transient and a mellow
body. It is hand-authored; a Yamaha factory bank is not bundled. Algorithm and
feedback controls can be changed during a note without restarting envelopes.
Output gain is smoothed over 15 ms. Fine tuning covers ±100 cents.

## Import

Import a Yamaha DX7 `.syx` single-voice VCED dump (163 bytes) or a 32-voice VMEM
bank (4104 bytes). A bank loads its **first voice**. The parser validates Yamaha
manufacturer and format IDs, payload length, seven-bit data, final terminator,
checksum, and decoded parameter bounds. A rejected import leaves the voice
unchanged. Raw 128-byte patches, performance banks, parameter-change messages,
DX7II extensions, and concatenated SysEx messages are not accepted.

The host wrapper stores the full 155-byte unpacked voice in project state. Call
`parseSysex()` on a non-audio thread, then transfer the fixed-size voice to the
audio thread and apply it with `setPatch()`. `setPatch()`, `patch()`, all MIDI
methods, and `setParameters()` require the same thread as `render()`. In
particular, do not read `patch()` from the UI during playback. Import resets
voices; sync the algorithm/feedback UI parameters to bytes 134/135 after import.

## Timing and fidelity

The engine runs at a fixed internal 49,096 Hz and uses 64-sample control periods
(about 1.30 ms). A 32-tap Blackman-windowed sinc converter adapts the audio to the
host sample rate and adds 15.5 internal samples of delay (reported rounded up by
`latencySamples()`: 16 host samples at 48 kHz). Note and control changes take effect
at the next internal block; the pending internal block may introduce up to 63
additional internal samples of timing quantization. Audio-block size does not
change pitch or envelope rates. Shared lookup-table initialization is thread safe
and independent of each instance's host sample rate.

This is a credible established digital DX7 model, not a new approximate six-sine
FM sketch. **Its accuracy has not been measured against a physical DX7 in this
project.** Analog reconstruction/DAC noise, per-unit calibration, hardware MIDI
throughput, microtuning, channel aftertouch, breath, mod-wheel performance
assignments, and original hardware SysEx transmission are not modeled by this
adapter. Imported voice LFO depths and sensitivities are processed. The MkI
upstream implementation itself describes parts of its amplitude modulation and
multi-operator feedback as approximations.

## Validation

`testHex()` checks A4 pitch and octave bend, silence before MIDI, note/release and
sustain behavior, VCED and VMEM conversion, malformed SysEx rejection, all 32
algorithms with feedback and voice stealing, finite output from 8–192 kHz, and
sample-rate isolation between multiple instances. These are software behavior
checks, not a hardware null test.
