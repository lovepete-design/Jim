#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
export PATH="/Applications/CMake.app/Contents/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-3}"
if ! command -v cmake >/dev/null; then
    echo 'Install CMake first: brew install cmake'
    echo 'Alternatively install the macOS application from https://cmake.org/download/'
    read -r -p 'Press Return to close.'
    exit 1
fi
bash scripts/build-macos.sh
bash scripts/install-macos.sh
echo 'Finished. Restart your VST3-compatible DAW and rescan plugins.'
read -r -p 'Press Return to close.'
