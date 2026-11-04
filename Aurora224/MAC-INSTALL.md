# Aurora 224 — Mac installation

The release ZIP contains a compiled universal VST3 for macOS 11 or newer, supporting Intel and Apple Silicon. No Xcode, CMake, or compiler is needed to install it.

1. Quit Ableton Live (or your VST3 host) and extract the ZIP.
2. Open Terminal, type `bash ` with a trailing space, drag `Install-Mac.command` into the Terminal window, and press Return.
3. Reopen Ableton. Enable VST3 System Folders in Settings → Plug-Ins. Hold Option while clicking Rescan if needed. Search for **Aurora 224**.
4. Use your host's generic parameter view for controls; this version has no custom editor.

The installer checks the bundled ad hoc signature, installs into `~/Library/Audio/Plug-Ins/VST3/`, verifies the installed signature, and removes download quarantine from this plugin only. Any older Aurora224 bundle is backed up under `~/Library/Application Support/Triad Audio/Plugin Backups/`.

This is locally signed, not Developer ID signed or Apple notarized. Build verification does not mean Apple notarization or compatibility with every DAW. The CI release is published only after the universal bundle passes the official Steinberg validator and DSP tests natively on both Intel and Apple Silicon runners. Logs are included. Logic Pro cannot load VST3 and requires a separate Audio Unit version.

An original Lexicon 224-inspired stereo reverb, independent of Lexicon. See the source README for DSP details. Source and licenses are included.
