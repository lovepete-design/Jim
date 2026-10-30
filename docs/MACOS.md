# macOS VST3 build and installation

The macOS build has not been run in this Linux workspace. The source and workflow target universal binaries for Apple Silicon (`arm64`) and Intel (`x86_64`), with macOS 11 as the deployment target. Build and test on a Mac before treating these as verified macOS plugins.

## Build on a Mac

Install Apple's command line tools:

```sh
xcode-select --install
```

Install CMake with Homebrew (`brew install cmake`) or its official macOS installer. From the source directory, run:

```sh
bash scripts/build-macos.sh
```

The build produces `Hex MkI.vst3`, `Stress.vst3`, and `Lattice.vst3` in `dist/macos`, then packages them with license notices and the corresponding source in `dist/triad-audio-macos.zip`. Dependencies are vendored or fetched by CMake at pinned revisions; an initial build may need internet access. The script runs the DSP and VST3 host smoke tests on the build machine's native architecture. An architecture list in a universal binary does not establish successful execution on both types of Mac.

The bundles receive an **ad hoc signature**. They have no Developer ID signature and are **not notarized**. Release distribution outside a development setup requires your own Apple signing identity and notarization process. The script does not change Gatekeeper settings or remove quarantine attributes.

Useful build overrides:

```sh
# Build only for the current Apple Silicon Mac.
TRIAD_ARCHS=arm64 bash scripts/build-macos.sh

# Limit compile jobs on a memory-constrained machine.
CMAKE_BUILD_PARALLEL_LEVEL=4 bash scripts/build-macos.sh
```

`TRIAD_BUILD_DIR` changes the build directory. `TRIAD_DEPLOYMENT_TARGET` changes the minimum macOS version. Use a fresh build directory when changing toolchains or architecture settings. The default remains a universal build.

## Install explicitly

Close your DAW, then run:

```sh
bash scripts/install-macos.sh
```

For an extracted release package, run its `install-macos.sh` instead. The installer copies the three bundles into `~/Library/Audio/Plug-Ins/VST3`. Existing bundles with the same names move to a timestamped `.triad-backups` directory inside that folder before replacement. No administrator access is required. Restart the DAW and rescan VST3 plugins.

Use a macOS DAW with VST3 support, such as REAPER, Ableton Live, Cubase, or Studio One. Logic Pro requires Audio Units; these VST3 bundles do not load in Logic Pro.

To remove a plugin, close the host and move its `.vst3` bundle out of the user VST3 folder. To restore a previous version, replace it with the corresponding saved bundle from `.triad-backups`.

## GitHub Actions

The `macOS VST3` workflow builds on a macOS runner when dispatched manually, on a push to `main` or `work`, and for pull requests. It uploads `triad-audio-macos-universal`, containing the package ZIP. The workflow has been prepared here but has not been submitted or executed.

## Validation still needed on macOS

Open each plugin in your target DAW, confirm audio and MIDI operation, save and reopen a session, exercise automation, and verify both Apple Silicon and Intel execution where supported. Hardware fidelity requires reference audio and measurements; a successful build does not establish an exact DX7 or Distressor match.
