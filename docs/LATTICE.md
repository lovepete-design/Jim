# Lattice — phase-gradient energy synthesis

Lattice is an original experimental instrument built for this project. It combines a network of sinusoidal modes, phase coupling, and a moving energy distribution. It is a distinct implementation and design proposal; no claim of historically unprecedented synthesis is made. Coupled oscillators, additive synthesis, and reaction-diffusion systems all have prior art.

Each MIDI voice has twelve modes. The fundamental is anchored to equal-tempered MIDI pitch. Higher modes start near integer harmonics and stretch with **Tension**. A ring connects adjacent modes; its end connects back to its start. The phase relationships determine where energy tends to collect, while energy gradients alter oscillator frequency. This feedback creates timbral motion from the note itself, without a recording, wavetable, random LFO, or a filter sweep.

For mode `i`, let `E_i` be nonnegative normalized energy and `theta_i` be oscillator phase. Every 16 samples:

```
agreement_i = 0.5 + 0.25 * [cos(theta_i - theta_(i-1))
                          + cos(theta_i - theta_(i+1))]
preferred_i = (i+1)^(-spectralSlope) * [0.08 + 0.92 * agreement_i^2]
E'_i = E_i + diffusionStep * (E_(i-1) + E_(i+1) - 2E_i)
           + reactionStep * (preferred_i - E_i)
E_i = max(E'_i, epsilon) / sum(max(E'_j, epsilon))
```

The reaction time follows **Memory**. **Diffusion** spreads energy and strengthens neighboring phase pulls. The higher-mode frequency is its stretched harmonic frequency plus a small bounded term derived from the sine of neighboring phase differences and the energy gradient. The fundamental receives no frequency pull. Mode amplitude follows the square root of energy. Fixed, equal-power stereo positions separate modes across the sound field.

The oscillator uses a normalized complex recurrence at audio rate; the network and coefficient calculations run at sample-rate-derived control intervals. Parameters and pitch bend smooth over roughly 25 ms. The instrument supports 12 voices, velocity, a ±48-semitone internal bend API (the host wrapper selects the MIDI bend range), sustain pedal, and quietest-release-first voice stealing. Voice stealing replaces an existing oscillator state, so extremely dense repeated notes may produce a short transient. Audio processing allocates no memory.

Modes fade between 0.33 and 0.43 times sample rate and disappear above that range. Frequency coupling and output saturation can still produce some aliasing: this is an experimental synthesis engine, not a claim of mathematically alias-free operation. A fixed `tanh` output saturator bounds extreme chords to ±1. It is nearly linear at typical levels. Output is not automatically loudness matched across patches.

## Controls

| Control | Effect |
| --- | --- |
| Diffusion | Smears energy around the ring and increases phase coupling. |
| Tension | Stretches upper modes into inharmonic ratios and increases energy-gradient coupling. |
| Memory | Controls how slowly the energy distribution follows current phase relationships. |
| Brightness | Changes the preferred spectral energy slope. |
| Attack | Plugin envelope onset, 1–2000 ms. The envelope reaches 99% in the indicated time. |
| Release | Plugin envelope decay, 20–6000 ms. It reaches −80 dB in the indicated time, then becomes silent. |
| Output | Gain before the fixed safety saturation, −36 to +6 dB. |

## Starting points

| Sound | Diffusion | Tension | Memory | Brightness | Attack | Release | Output |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Violet currents | 0.35 | 0.25 | 0.50 | 0.50 | 20 ms | 700 ms | −9 dB |
| Glass roots | 0.10 | 0.15 | 0.20 | 0.80 | 2 ms | 1300 ms | −9 dB |
| Slow weather | 0.80 | 0.40 | 0.95 | 0.30 | 900 ms | 4500 ms | −9 dB |
| Wire bloom | 0.55 | 0.85 | 0.65 | 0.75 | 35 ms | 1800 ms | −12 dB |

One held note makes the internal motion easiest to hear. Higher Tension settings deliberately change the harmonic relationships, while the MIDI fundamental remains in tune. CPU work scales with active voices (12 modes each); long releases and large chords cost more than monophonic playing.
