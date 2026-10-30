# DX7 engine attribution

HEX includes a selected DSP subset of [Dexed](https://github.com/asb2m10/dexed),
release `v0.9.4`, commit `618b318792e74386b7afc910d1365827e770222e`.
The files are stored in `third_party/msfa/`. No Dexed artwork, factory voice bank,
JUCE wrapper, or tuning library is included in that subset.

- `EngineMkI.cpp`: Copyright 2015–2017 Pascal Gauthier, GNU GPL version 3 or later.
  Its header carries Apache-2.0 terms; the implementation makes the linked HEX
  distribution subject to GPL-3.0-or-later.
- The other MSFA files: Copyright Google Inc. and Pascal Gauthier as specified in
  each source header, Apache License 2.0. These descend from Google's
  [Music Synthesizer for Android](https://github.com/google/music-synthesizer-for-android).
- License texts: `licenses/Dexed-GPL-3.0.txt` and
  `licenses/MSFA-Apache-2.0.txt`.

Local changes are marked `Triad` in the affected source files:

1. Remove the framework-only `Dexed.h` includes and disable debug tracing.
2. Replace obsolete Apple OSAtomic memory barriers with standard C++ atomics.
3. Initialize note feedback state deterministically and expose a feedback reset.
4. Initialize shared MkI logarithmic-sine lookup tables once with `std::call_once`.
5. Expose algorithm/feedback changes without resetting an active envelope.

`HexEngine.cpp` is a new adapter, MIDI/voice manager, original default voice, strict
SysEx parser, and a causal 32-tap/1024-phase Blackman-windowed sinc resampler.
All shared sample-rate-dependent MSFA tables are initialized once at a fixed
49,096 Hz internal rate; host-rate conversion is private to each adapter instance.
Use `-fwrapv` with GCC or Clang for the upstream fixed-point phase arithmetic.

Yamaha and DX7 are trademarks of their respective owners. HEX is independently
packaged software and is not endorsed by Yamaha. No claim of measured analog
output-stage equivalence or bit-for-bit hardware validation is made.
