#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'This script builds macOS plugins. Run it on a Mac.'
  exit 1
fi
if ! command -v cmake >/dev/null; then
  echo 'Install CMake (https://cmake.org/download/ or brew install cmake), then run this script again.'
  exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
  echo 'Install Apple developer tools with: xcode-select --install'
  exit 1
fi
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-mac --config Release --parallel 3
ctest --test-dir build-mac -C Release --output-on-failure
mkdir -p dist
ditto 'build-mac/TriadTune_artefacts/Release/VST3/Triad Tune.vst3' 'dist/Triad Tune.vst3'
ditto 'build-mac/TriadTune_artefacts/Release/AU/Triad Tune.component' 'dist/Triad Tune.component'
ditto 'build-mac/TriadTune_artefacts/Release/Standalone/Triad Tune.app' 'dist/Triad Tune.app'
for bundle in 'dist/Triad Tune.vst3' 'dist/Triad Tune.component' 'dist/Triad Tune.app'; do
  codesign --force --deep --sign - "$bundle"
done
cp scripts/Install.command dist/Install.command
chmod +x dist/Install.command
cp README.md dist/README.md
ditto -c -k --sequesterRsrc --keepParent dist Triad-Tune-Mac.zip
echo 'Build finished. Run dist/Install.command to install the plugins.'
open dist
