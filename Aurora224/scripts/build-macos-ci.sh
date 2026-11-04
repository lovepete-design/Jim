#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
sdk="$root/.mac-build-tools/vst3sdk"
mkdir -p "$(dirname "$sdk")" dist
if [[ ! -d "$sdk/.git" ]]; then git clone https://github.com/steinbergmedia/vst3sdk.git "$sdk"; fi
git -C "$sdk" checkout --detach 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
git -C "$sdk" submodule update --init --depth 1 base cmake pluginterfaces public.sdk
cmake -S . -B build-mac -G 'Unix Makefiles' -DVST3_SDK_ROOT="$sdk" \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --config Release --parallel 4 2>&1 | tee dist/build-macos.log
ctest --test-dir build-mac -C Release --output-on-failure 2>&1 | tee dist/dsp-native.log
plugin="$root/build-mac/VST3/Release/Aurora224.vst3"
validator="$root/build-mac/bin/Release/validator"
test -d "$plugin"
test -x "$validator"
lipo "$plugin/Contents/MacOS/Aurora224" -verify_arch arm64 x86_64
codesign --force --sign - "$plugin"
codesign --verify --strict "$plugin"
"$validator" "$plugin" 2>&1 | tee dist/validator-native.log
mkdir -p dist/Aurora224-macOS/verification-tools
ditto "$plugin" dist/Aurora224-macOS/Aurora224.vst3
cp "$validator" build-mac/reverb_tests dist/Aurora224-macOS/verification-tools/
cp scripts/Install-Mac.command dist/Aurora224-macOS/
cp MAC-INSTALL.md LICENSE docs/VST3-SDK-LICENSE.txt dist/Aurora224-macOS/
cp dist/validator-native.log dist/dsp-native.log dist/Aurora224-macOS/
(cd "$root/.." && git archive HEAD Aurora224 .github/workflows/aurora224-macos.yml) > dist/Aurora224-macOS/source.tar
/usr/bin/ditto -c -k --sequesterRsrc --keepParent dist/Aurora224-macOS dist/Aurora224-macOS.zip
