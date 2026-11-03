#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Run this installer on macOS.'
  exit 1
fi
if [[ ! -d 'Triad Tune.vst3' ]]; then
  echo 'Keep Install.command next to the compiled Triad Tune.vst3 bundle.'
  exit 1
fi
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
ditto 'Triad Tune.vst3' "$HOME/Library/Audio/Plug-Ins/VST3/Triad Tune.vst3"
if [[ -d 'Triad Tune.component' ]]; then
  ditto 'Triad Tune.component' "$HOME/Library/Audio/Plug-Ins/Components/Triad Tune.component"
fi
echo 'Installed Triad Tune. In Ableton Settings > Plug-Ins, enable VST3 system folders and rescan.'
echo 'Press Return to close.'
read -r
