# Larry's Sexy Reverb II — separate Mac plugin

Universal Intel + Apple Silicon VST3, macOS 11 or newer. This is a separate plugin with new VST3 processor/controller IDs, bundle identifier, executable, filename, and native editor class names. It installs alongside Larry's Sexy Reverb and Aurora 224; older projects keep using their original plugins.

1. Quit Ableton Live and extract this ZIP.
2. Open Terminal, type `bash ` (with a space), drag `Install-Mac.command` into Terminal, and press Return.
3. Restart Live, enable Use VST3 Plug-In System Folders in Settings → Plug-Ins, then hold Option while clicking Rescan.
4. Search for **Larry's Sexy Reverb II** and open its custom editor.

The installer verifies the local ad hoc signature, copies only Larry's Sexy Reverb II.vst3 into ~/Library/Audio/Plug-Ins/VST3, verifies it again, and clears its download quarantine. It backs up an existing II installation only. No Xcode is needed. This is not Apple-notarized.

The editor has ten preset buttons, Hall/Plate/Chamber modes, six vertical faders, bypass, 100% wet, reset, and a second control page. PAGE switches to diffusion/bass/width. Double-click a fader to reset it. Host automation updates the display.

Audio, VST3 validator, native editor lifecycle, preset, control, and automation tests run on Intel and Apple Silicon before publication. A separate plugin identity allows a fresh scan; DAW-specific loading errors still require the exact host message to diagnose.
