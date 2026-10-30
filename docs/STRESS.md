# Stress — character dynamics

Stress is an independent stereo-linked compressor inspired by the control range and harmonic options of classic aggressive studio compressors, including the Empirical Labs Distressor. It is not affiliated with Empirical Labs. No hardware measurements, schematics, or proprietary algorithms were used; it should not be described as a calibrated hardware emulation.

## Controls

| Control | Behavior |
| --- | --- |
| Input | Drives a fixed −18 dBFS threshold. Raising Input produces more gain reduction. |
| Output | Manual gain after compression and parallel mixing. |
| Attack | 0.05–30 ms envelope time constant in the plugin. Short settings catch transients; long settings retain the initial hit. |
| Release | 40–3500 ms base recovery time in the plugin. 10:1 adds slower recovery in response to sustained gain reduction. |
| Ratio | 1:1, 2:1, 3:1, 4:1, 6:1, 10:1, 20:1, and 100:1. 1:1 allows harmonic coloration without compression; 100:1 approaches limiting after its attack interval. |
| Mix | Blends processed and uncompressed, Input-driven audio before Output gain. |
| Harmonics | Clean, H2 (asymmetric saturation), or H3 (symmetric saturation). |
| Detector HP | An 80 Hz highpass in the detector reduces bass triggering; it does not filter the audio path. |
| Emphasis | A broad high-frequency boost in the detector makes bright transients trigger more compression. |

The stereo detector follows the louder channel and applies one gain to both channels. There is no automatic makeup gain or lookahead. This is a creative compressor, not a brickwall safety limiter: fast peaks may exceed the threshold.

## Starting settings

| Use | Input | Output | Ratio | Attack | Release | Harmonics | Detector |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Drum punch | +9 dB | +3 dB | 4:1 | 12 ms | 90 ms | H3 | HP on |
| Vocal hold | +6 dB | +2 dB | 6:1 | 3 ms | 180 ms | H2 | Emphasis on |
| Slow squeeze | +12 dB | +5 dB | 10:1 | 8 ms | 350 ms | H2 | HP off |
| Parallel crush | +18 dB | 0 dB | 20:1 | 0.1 ms | 70 ms | H3 | HP on; Mix 25% |
| Color only | +3 dB | −3 dB | 1:1 | 5 ms | 100 ms | H2 | Either |

Adjust Input against the gain-reduction meter and match Output by ear. These are suggested starting settings, not measurements of hardware presets.

## Implementation and limits

The detector blends peak and short-window energy estimates. A 6 dB soft knee feeds a gain envelope in decibels. Continuous parameters and mode changes are smoothed over approximately 15 ms. H2 and H3 use first-order antiderivative antialiasing and DC blocking; aliasing is reduced, not eliminated, and high drive can still produce aliasing. The antiderivative stage has an approximately half-sample smoothing effect. No whole-sample host latency is reported, and the dry blend is not phase-compensated for that fractional effect.

The DSP uses no heap allocations, locks, or file I/O in processing. Invalid samples are replaced with zero and unusually large inputs are bounded to protect the internal detector. Gain reduction is published through a relaxed atomic meter for the UI. Parameter updates and prepare/reset belong on the audio thread or while processing is stopped.
