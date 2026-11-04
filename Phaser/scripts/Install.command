#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Run this installer on macOS.'
  exit 1
fi
if [[ ! -d 'Triad Phase.vst3' ]]; then
  echo 'Keep Install.command beside the extracted Triad Phase.vst3 bundle.'
  exit 1
fi
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
ditto 'Triad Phase.vst3' "$HOME/Library/Audio/Plug-Ins/VST3/Triad Phase.vst3"
if [[ -d 'Triad Phase.component' ]]; then
  ditto 'Triad Phase.component' "$HOME/Library/Audio/Plug-Ins/Components/Triad Phase.component"
fi
echo 'Installed Triad Phase.'
echo 'Restart Ableton. In Settings > Plug-Ins, enable VST3 system folders and click Rescan.'
echo 'Find Triad Phase under Plug-Ins > VST3 > Triad Audio.'
echo 'Press Return to close.'
read -r
