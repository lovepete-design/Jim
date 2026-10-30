#!/bin/bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
source_package="$project_root/dist/macos"
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This packaging script requires macOS.' >&2
    exit 1
fi
if [[ ! -f "$source_package/triad-audio-source.tar.gz" || ! -d "$source_package/Hex MkI.vst3" ]]; then
    echo 'Run scripts/build-macos.sh successfully before packaging the Hex update.' >&2
    exit 1
fi
package_stage="$(mktemp -d "$project_root/dist/.hex-stage.XXXXXX")"
trap 'rm -rf "$package_stage"' EXIT
ditto "$source_package/Hex MkI.vst3" "$package_stage/Hex MkI.vst3"
codesign --verify --deep --strict "$package_stage/Hex MkI.vst3"
lipo -verify_arch arm64 x86_64 "$package_stage/Hex MkI.vst3/Contents/MacOS/Hex MkI"
cp "$project_root/scripts/install-hex-macos.sh" "$package_stage/install-hex-macos.sh"
cp "$project_root/docs/HEX-LIBRARY.md" "$package_stage/START-HERE.md"
cp "$source_package/triad-audio-source.tar.gz" "$package_stage/triad-audio-source.tar.gz"
cp "$project_root/LICENSE" "$project_root/THIRD_PARTY.md" "$project_root/THIRD_PARTY_DX7.md" "$package_stage/"
cp "$source_package/BUILD-INFO.txt" "$package_stage/BUILD-INFO.txt"
ditto "$project_root/licenses" "$package_stage/licenses"
package_dir="$project_root/dist/hex-macos"
if [[ -e "$package_dir" ]]; then
    mv "$package_dir" "$project_root/dist/hex-macos.previous-$(date +%Y%m%d-%H%M%S)-$$"
fi
mv "$package_stage" "$package_dir"
trap - EXIT
ditto -c -k --keepParent "$package_dir" "$project_root/dist/Hex-MkI-macOS.zip"
(cd "$project_root/dist" && shasum -a 256 Hex-MkI-macOS.zip > Hex-MkI-macOS.zip.sha256)
echo "Built Hex update: $project_root/dist/Hex-MkI-macOS.zip"
