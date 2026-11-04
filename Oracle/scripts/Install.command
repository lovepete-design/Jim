#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Run this installer on macOS.'
  exit 1
fi
if [[ ! -d 'Triad Oracle.vst3' ]]; then
  echo 'Keep Install.command beside the extracted Triad Oracle.vst3 bundle.'
  exit 1
fi
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
ditto 'Triad Oracle.vst3' "$HOME/Library/Audio/Plug-Ins/VST3/Triad Oracle.vst3"
if [[ -d 'Triad Oracle.component' ]]; then
  ditto 'Triad Oracle.component' "$HOME/Library/Audio/Plug-Ins/Components/Triad Oracle.component"
fi
echo 'Installed Triad Oracle.'
echo 'Restart Ableton, enable VST3 system folders in Settings > Plug-Ins, then click Rescan.'
echo 'Load Triad Oracle onto a MIDI track. Arm that track, play MIDI notes, or click its on-screen keys.'
echo 'Press Return to close.'
read -r
