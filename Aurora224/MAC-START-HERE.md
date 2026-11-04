# Aurora 224 for Mac

This package contains source and a build-and-install launcher, **not a precompiled Mac plugin**. The launcher targets Apple Silicon and Intel, requires macOS 11 or later, and uses Apple's Command Line Tools. Internet access is required to download the official VST3 SDK and CMake if needed. No paid developer account or Homebrew is required.

1. Extract the ZIP onto your Mac.
2. Quit your DAW and double-click `Build-and-Install-Mac.command`.
3. If Apple asks you to install Command Line Tools, finish that installation and run the launcher again.
4. After the build succeeds, restart your DAW and rescan VST3 plugins. Look for **Aurora 224** and open your DAW's generic parameter view.

The launcher compiles a universal VST3, runs the SDK validator and DSP tests, checks both CPU architectures, signs the bundle locally, and installs it into `~/Library/Audio/Plug-Ins/VST3/Aurora224.vst3`. Any existing Aurora224 bundle is moved into this project's `plugin-backups` folder first. Downloaded build tools and SDK stay in this project's `.mac-build-tools` directory.

If Finder does not open the launcher, open Terminal, type `bash ` (including the space), drag `Build-and-Install-Mac.command` into Terminal, and press Return.

**Logic Pro requires Audio Units and cannot load this VST3.** Ableton Live, REAPER, Cubase, and other VST3 hosts can use this format. The plugin has no custom editor.

The Mac launcher has been checked for shell syntax on Linux; a macOS build has not been run in this environment. The previous Linux binary is not compatible with Mac.
