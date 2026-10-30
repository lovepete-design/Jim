#!/bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This installer requires macOS.' >&2
    exit 1
fi
if [[ -d "$script_dir/Hex MkI.vst3" ]]; then
    # Installer copied into an extracted release package.
    package_dir="$script_dir"
else
    package_dir="${TRIAD_PACKAGE_DIR:-$script_dir/../dist/macos}"
fi
plugin_dir="$HOME/Library/Audio/Plug-Ins/VST3"
backup_dir="$plugin_dir/.triad-backups/$(date +%Y%m%d-%H%M%S)-$$"
products=('Hex MkI' Stress Lattice)

for product in "${products[@]}"; do
    if [[ ! -d "$package_dir/$product.vst3/Contents" ]]; then
        echo "Missing $package_dir/$product.vst3. Build or extract the macOS release first." >&2
        exit 1
    fi
    codesign --verify --deep --strict "$package_dir/$product.vst3"
done
mkdir -p "$plugin_dir"
for product in "${products[@]}"; do
    destination="$plugin_dir/$product.vst3"
    staged="$plugin_dir/.$product.vst3.install-$$"
    ditto "$package_dir/$product.vst3" "$staged"
    if [[ -e "$destination" ]]; then
        mkdir -p "$backup_dir"
        mv "$destination" "$backup_dir/$product.vst3"
        echo "Backed up $product to $backup_dir"
    fi
    mv "$staged" "$destination"
    echo "Installed $destination"
done
echo 'Restart your DAW and rescan VST3 plugins. Logic Pro does not host VST3.'
