#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
PROJECT_DIR="$PWD"
trap 'result=$?; if [ "$result" -ne 0 ]; then echo; echo "Build stopped. Read the error above; no new plugin was installed."; fi; if [ -t 0 ]; then read -r -p "Press Return to close…" ignored; fi' EXIT

if [ "$(uname -s)" != Darwin ]; then
    echo "Run this launcher on your Mac. It builds a macOS VST3, not a Linux plugin."
    exit 1
fi
if ! /usr/bin/xcrun --find clang++ >/dev/null 2>&1; then
    echo "Apple Command Line Tools are required. Opening Apple's installer."
    /usr/bin/xcode-select --install || true
    echo "Finish installing the tools, then run this launcher again."
    exit 1
fi
CACHE_DIR="$PROJECT_DIR/.mac-build-tools"
mkdir -p "$CACHE_DIR"
CMAKE_BIN="$(command -v cmake || true)"
if [ -z "$CMAKE_BIN" ]; then
    CMAKE_VERSION=3.31.6
    CMAKE_ARCHIVE="cmake-${CMAKE_VERSION}-macos-universal.tar.gz"
    CMAKE_BIN="$CACHE_DIR/cmake-${CMAKE_VERSION}-macos-universal/CMake.app/Contents/bin/cmake"
    if [ ! -x "$CMAKE_BIN" ]; then
        echo "Downloading universal CMake ${CMAKE_VERSION} from Kitware…"
        RELEASE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}"
        /usr/bin/curl --fail --location --retry 3 "$RELEASE_URL/$CMAKE_ARCHIVE" -o "$CACHE_DIR/$CMAKE_ARCHIVE"
        /usr/bin/curl --fail --location --retry 3 "$RELEASE_URL/cmake-${CMAKE_VERSION}-SHA-256.txt" -o "$CACHE_DIR/checksums.txt"
        ( cd "$CACHE_DIR"
          /usr/bin/awk -v file="$CMAKE_ARCHIVE" '$2 == file {print}' checksums.txt > selected-checksum.txt
          test -s selected-checksum.txt
          /usr/bin/shasum -a 256 -c selected-checksum.txt
          /usr/bin/tar -xzf "$CMAKE_ARCHIVE"
        )
    fi
fi
SDK_DIR="$CACHE_DIR/vst3sdk"
SDK_COMMIT=3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
if [ ! -d "$SDK_DIR/.git" ]; then
    /usr/bin/git clone https://github.com/steinbergmedia/vst3sdk.git "$SDK_DIR"
fi
/usr/bin/git -C "$SDK_DIR" checkout --detach "$SDK_COMMIT"
/usr/bin/git -C "$SDK_DIR" submodule update --init --depth 1 base cmake pluginterfaces public.sdk

BUILD_DIR="$PROJECT_DIR/build-mac"
echo "Building Aurora 224 for Apple Silicon and Intel…"
"$CMAKE_BIN" -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -G "Unix Makefiles" \
    -DVST3_SDK_ROOT="$SDK_DIR" \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_BUILD_TYPE=Release
"$CMAKE_BIN" --build "$BUILD_DIR" --config Release --parallel 4
"$(dirname "$CMAKE_BIN")/ctest" --test-dir "$BUILD_DIR" -C Release --output-on-failure
PLUGIN="$BUILD_DIR/VST3/Release/Aurora224.vst3"
test -d "$PLUGIN"
/usr/bin/lipo -verify_arch arm64 x86_64 "$PLUGIN/Contents/MacOS/Aurora224"
/usr/bin/codesign --force --sign - "$PLUGIN"
/usr/bin/codesign --verify --strict "$PLUGIN"
INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$INSTALL_DIR"
if [ -e "$INSTALL_DIR/Aurora224.vst3" ]; then
    BACKUP_DIR="$PROJECT_DIR/plugin-backups"
    mkdir -p "$BACKUP_DIR"
    mv "$INSTALL_DIR/Aurora224.vst3" "$BACKUP_DIR/Aurora224-$(date +%Y%m%d-%H%M%S)-$$.vst3"
fi
/usr/bin/ditto "$PLUGIN" "$INSTALL_DIR/Aurora224.vst3"
echo
echo "Installed: $INSTALL_DIR/Aurora224.vst3"
echo "Restart your DAW and rescan VST3 plugins. Open Aurora 224 using your DAW's parameter view."
echo "This VST3 does not load in Logic Pro, which requires Audio Units."
