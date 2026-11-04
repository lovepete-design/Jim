#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Run this build script on a Mac.'
  exit 1
fi
if ! command -v cmake >/dev/null || ! xcode-select -p >/dev/null 2>&1; then
  echo 'Install CMake and Xcode command line tools (xcode-select --install) first.'
  exit 1
fi
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-mac --parallel 3
ctest --test-dir build-mac --output-on-failure
mkdir -p dist
for spec in VST3:vst3 AU:component Standalone:app; do
  format="${spec%%:*}"; extension="${spec##*:}"
  ditto "build-mac/TriadPhase_artefacts/Release/$format/Triad Phase.$extension" "dist/Triad Phase.$extension"
  codesign --force --deep --sign - "dist/Triad Phase.$extension"
done
cp scripts/Install.command dist/Install.command
chmod +x dist/Install.command
cp README.md dist/README.md
cp build-mac/Triad-Phase-Preview.png dist/Triad-Phase-Preview.png
ditto -c -k --sequesterRsrc --keepParent dist Triad-Phase-Mac.zip
echo 'Build finished. Run dist/Install.command to install Triad Phase.'
open dist
