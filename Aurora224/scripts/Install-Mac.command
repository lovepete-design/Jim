#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
trap 'status=$?; if [ "$status" -ne 0 ]; then echo "Installation stopped. See the error above."; fi; if [ -t 0 ]; then read -r -p "Press Return to close…" ignored; fi' EXIT
plugin="$PWD/Aurora224.vst3"
test -d "$plugin"
# Verify the supplied local signature before installing or clearing its quarantine.
/usr/bin/codesign --verify --strict "$plugin"
destination="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$destination"
if [[ -e "$destination/Aurora224.vst3" ]]; then
    backup="$HOME/Library/Application Support/Triad Audio/Plugin Backups"
    mkdir -p "$backup"
    mv "$destination/Aurora224.vst3" "$backup/Aurora224-$(date +%Y%m%d-%H%M%S)-$$.vst3"
fi
/usr/bin/ditto "$plugin" "$destination/Aurora224.vst3"
/usr/bin/codesign --verify --strict "$destination/Aurora224.vst3"
/usr/bin/xattr -dr com.apple.quarantine "$destination/Aurora224.vst3"
echo "Installed Aurora 224. Restart Ableton Live and rescan VST3 plugins."
