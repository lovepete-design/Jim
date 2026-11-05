#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
trap 'status=$?; if [ "$status" -ne 0 ]; then echo "Installation stopped. See the error above."; fi; if [ -t 0 ]; then read -r -p "Press Return to close…" ignored; fi' EXIT
plugin_name="Larry's Sexy Reverb II.vst3"
plugin="$PWD/$plugin_name"
test -d "$plugin"
# Verify the supplied local signature before installing or clearing its quarantine.
/usr/bin/codesign --verify --strict "$plugin"
destination="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$destination"
if [[ -e "$destination/$plugin_name" ]]; then
    backup="$HOME/Library/Application Support/Triad Audio/Plugin Backups"
    mkdir -p "$backup"
    mv "$destination/$plugin_name" "$backup/Larrys-Sexy-Reverb-II-$(date +%Y%m%d-%H%M%S)-$$.vst3"
fi
/usr/bin/ditto "$plugin" "$destination/$plugin_name"
/usr/bin/codesign --verify --strict "$destination/$plugin_name"
/usr/bin/xattr -dr com.apple.quarantine "$destination/$plugin_name"
echo "Installed Larry's Sexy Reverb II. Restart Ableton Live and rescan VST3 plugins."
