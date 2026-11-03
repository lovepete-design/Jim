#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Run this build script on a Mac.'
  exit 1
fi
if ! command -v cmake >/dev/null || ! xcode-select -p >/dev/null 2>&1; then
  echo 'Install CMake and the Xcode command line tools first (xcode-select --install).'
  exit 1
fi
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-mac --parallel 3
ctest --test-dir build-mac --output-on-failure
mkdir -p dist
for target in TriadDeEss TriadMaster; do
  if [[ "$target" == TriadDeEss ]]; then product='Triad DeEss'; else product='Triad Master'; fi
  for spec in 'VST3:vst3' 'AU:component' 'Standalone:app'; do
    format="${spec%%:*}"; extension="${spec##*:}"
    ditto "build-mac/${target}_artefacts/Release/$format/$product.$extension" "dist/$product.$extension"
    codesign --force --deep --sign - "dist/$product.$extension"
  done
done
cp scripts/Install.command dist/Install.command
cp README.md dist/README.md
chmod +x dist/Install.command
ditto -c -k --sequesterRsrc --keepParent dist Triad-Effects-Mac.zip
echo 'Build finished. Open dist/Install.command to install both plugins.'
open dist
