#!/bin/bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${TRIAD_BUILD_DIR:-$project_root/build-macos}"
architectures="${TRIAD_ARCHS:-arm64;x86_64}"
deployment_target="${TRIAD_DEPLOYMENT_TARGET:-11.0}"

if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This script requires macOS and the Apple command line tools.' >&2
    exit 1
fi
for tool in cmake ctest xcrun codesign ditto; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing $tool. See docs/MACOS.md for setup." >&2
        exit 1
    fi
done
xcrun --find clang++ >/dev/null

cmake -S "$project_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_OSX_ARCHITECTURES=$architectures" \
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=$deployment_target"
cmake --build "$build_dir" --config Release \
    --target HexMkI_VST3 Stress_VST3 Lattice_VST3 triad_tests triad_host triad_library_tests triad_hex_state_tests \
    --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.logicalcpu)}"
# The manifest helper writes resources after the initial link-time signature.
# Refresh the final bundles before loading them in the validation host.
targets=(HexMkI Stress Lattice)
products=('Hex Library' Stress Lattice)
for index in 0 1 2; do
    built_bundle="$build_dir/${targets[$index]}_artefacts/Release/VST3/${products[$index]}.vst3"
    codesign --force --deep --sign - "$built_bundle"
    codesign --verify --deep --strict "$built_bundle"
done
ctest --test-dir "$build_dir" -C Release --output-on-failure

mkdir -p "$project_root/dist"
package_stage="$(mktemp -d "$project_root/dist/.macos-stage.XXXXXX")"
trap 'rm -rf "$package_stage"' EXIT

targets=(HexMkI Stress Lattice)
products=('Hex Library' Stress Lattice)
for index in 0 1 2; do
    bundle="${products[$index]}.vst3"
    source_bundle="$build_dir/${targets[$index]}_artefacts/Release/VST3/$bundle"
    if [[ ! -d "$source_bundle" ]]; then
        echo "Expected VST3 bundle not found: $source_bundle" >&2
        exit 1
    fi
    ditto "$source_bundle" "$package_stage/$bundle"
    codesign --force --deep --sign - "$package_stage/$bundle"
    codesign --verify --deep --strict "$package_stage/$bundle"
    binary="$package_stage/$bundle/Contents/MacOS/${products[$index]}"
    if [[ ! -f "$binary" ]]; then
        echo "Expected plugin executable not found: $binary" >&2
        exit 1
    fi
    echo "$bundle: $(lipo -archs "$binary")"
done

# Bundle the license notices and corresponding source with the binaries.
cp "$project_root/LICENSE" "$package_stage/LICENSE"
for notice in README.md THIRD_PARTY.md THIRD_PARTY_DX7.md; do
    if [[ -f "$project_root/$notice" ]]; then
        cp "$project_root/$notice" "$package_stage/$notice"
    fi
done
ditto "$project_root/licenses" "$package_stage/licenses"
ditto "$project_root/docs" "$package_stage/docs"
cp "$project_root/scripts/install-macos.sh" "$package_stage/install-macos.sh"
tar -czf "$package_stage/triad-audio-source.tar.gz" \
    --exclude=.git --exclude='./dist' --exclude='./build*' \
    -C "$project_root" .
cat > "$package_stage/BUILD-INFO.txt" <<EOF
Triad Audio macOS VST3 build
Architectures: $architectures
Minimum macOS target: $deployment_target
Signing: local ad hoc signature; no Developer ID; not notarized.
Tests: DSP and VST3 host tests passed on the build machine's native architecture.
Host compatibility and comparison against reference hardware require separate testing.
Corresponding source: triad-audio-source.tar.gz
EOF

package_dir="$project_root/dist/macos"
if [[ -e "$package_dir" ]]; then
    mv "$package_dir" "$project_root/dist/macos.previous-$(date +%Y%m%d-%H%M%S)-$$"
fi
mv "$package_stage" "$package_dir"
trap - EXIT
ditto -c -k --keepParent "$package_dir" "$project_root/dist/triad-audio-macos.zip"
echo "Built macOS package: $project_root/dist/triad-audio-macos.zip"
echo 'Run scripts/install-macos.sh explicitly to install into your user VST3 folder.'
