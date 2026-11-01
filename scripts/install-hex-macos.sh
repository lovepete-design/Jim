#!/bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This installer requires macOS.' >&2
    exit 1
fi
if pgrep -x Live >/dev/null 2>&1; then
    echo 'Quit Ableton Live, then run this installer again.' >&2
    exit 1
fi
if [[ -d "$script_dir/Hex Library.vst3" ]]; then
    package_dir="$script_dir"
else
    package_dir="${TRIAD_PACKAGE_DIR:-$script_dir/../dist/hex-macos}"
fi
source_bundle="$package_dir/Hex Library.vst3"
if [[ ! -d "$source_bundle/Contents" || ! -x "$source_bundle/Contents/MacOS/Hex Library" ]]; then
    echo "Missing Hex Library.vst3 in $package_dir. Extract the complete release first." >&2
    exit 1
fi
codesign --verify --deep --strict "$source_bundle"

plugin_dir="$HOME/Library/Audio/Plug-Ins/VST3"
destination="$plugin_dir/Hex Library.vst3"
backup_dir="$plugin_dir/.triad-backups/$(date +%Y%m%d-%H%M%S)-$$"
staged="$plugin_dir/.Hex Library.vst3.install-$$"
backed_up=0
installed=0
cleanup() {
    result=$?
    if [[ -d "$staged" ]]; then rm -rf "$staged"; fi
    if [[ "$installed" == 0 && "$backed_up" == 1 && ! -e "$destination" ]]; then
        mv "$backup_dir/Hex Library.vst3" "$destination"
        echo 'Restored the previous Hex Library after installation failed.' >&2
    fi
    exit "$result"
}
trap cleanup EXIT
mkdir -p "$plugin_dir"
ditto "$source_bundle" "$staged"
codesign --verify --deep --strict "$staged"
# Only this verified, staged plugin is affected. Nothing changes system policy.
xattr -dr com.apple.quarantine "$staged" 2>/dev/null || true
if xattr -lr "$staged" | grep -F 'com.apple.quarantine' >/dev/null; then
    echo 'Could not clear the download quarantine on Hex Library.' >&2
    exit 1
fi
codesign --verify --deep --strict "$staged"
if [[ -e "$destination" ]]; then
    mkdir -p "$backup_dir"
    mv "$destination" "$backup_dir/Hex Library.vst3"
    backed_up=1
fi
mv "$staged" "$destination"
installed=1
echo "Installed $destination"
if [[ "$backed_up" == 1 ]]; then echo "Previous version saved in $backup_dir"; fi
echo 'Open Ableton Live and load Hex Library. Click Import Folder once to import your sound collection.'
echo 'If Live still shows the old version, hold Option while clicking Rescan in Settings > Plug-Ins.'
