#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Run this installer on macOS.'
  exit 1
fi
installed=0
for product in 'Triad DeEss' 'Triad Master'; do
  if [[ -d "$product.vst3" ]]; then
    mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
    ditto "$product.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/$product.vst3"
    installed=$((installed + 1))
  fi
  if [[ -d "$product.component" ]]; then
    mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
    ditto "$product.component" "$HOME/Library/Audio/Plug-Ins/Components/$product.component"
  fi
done
if [[ "$installed" -eq 0 ]]; then
  echo 'Keep Install.command beside the extracted Triad plugin bundles.'
  exit 1
fi
echo "Installed $installed Triad VST3 plugin(s)."
echo 'Restart Ableton. In Settings > Plug-Ins, enable VST3 system folders and click Rescan.'
echo 'Find the plugins under Plug-Ins > VST3 > Triad Audio.'
echo 'Press Return to close.'
read -r
