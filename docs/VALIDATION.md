# Validation — 2026-10-30

Built and tested on Debian 13, Linux x86_64, GCC 14.2, JUCE 8.0.6.

## Passed

* Complete DSP suite: Hex MkI, Stress, and Lattice.
* All three compiled Linux VST3 bundles discovered and instantiated by a JUCE
  VST3 host, with stereo audio and sample-offset MIDI checks for instruments.
* Native custom editors opened and captured under Xvfb.
* Parameter enumeration, normalized values, and automation at range extremes.
* State recall after 40 preset changes without processing audio; recreation of
  a second instance from saved state; matching deterministic renders.
* Instance isolation and restart/render checks at 44.1, 48, and 96 kHz.
* Individual DSP checks covered sustain, release, pitch bend, rate extremes,
  malformed SysEx, numerical stability, compressor linking/ratios/harmonics,
  and Lattice's Nyquist taper and audible control response.
* Individual address-sanitizer runs passed. Stress and Lattice also passed
  undefined-behavior sanitization. LeakSanitizer could not run under this host's
  ptrace restrictions. The upstream DX7 core uses intentional fixed-point wrap.
* Mac shell scripts pass shell syntax checks.

The validation host exposed a Linux shared-symbol collision between plugin
libraries. Linux bundles now hide archive symbols with `--exclude-libs,ALL`;
all three load and run in the same host after the fix. The final validator and
wrapper links were run directly against the compiled objects to avoid repeated
rebuilds from this workspace's damaged Ninja dependency cache. The equivalent
link option is included in CMake.

Screenshots and PCM WAV demonstrations are in `previews/`. The font system
reported an unwritable cache directory in this restricted host; fonts rendered
successfully.

## Not established

macOS universal compilation, signing, installation, and DAW compatibility have
not been executed here: this machine has no Apple SDK or toolchain. The Mac
script and CI workflow build and validate on a Mac. A universal file must still
be tested on both processor architectures for full coverage. Linux binaries
cannot load on macOS.

These are development builds. Hardware comparison/null tests, Distressor
calibration, external commercial-DAW validation, and claims of historically
unprecedented synthesis have not been established.
