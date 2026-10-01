#!/bin/bash
# Installs VoxSlap for the current user.
set -e
cd "$(dirname "$0")"
mkdir -p ~/.vst3 ~/.local/bin
rm -rf ~/.vst3/VoxSlap.vst3
cp -R VoxSlap.vst3 ~/.vst3/
[ -f VoxSlap ] && cp VoxSlap ~/.local/bin/VoxSlap && chmod +x ~/.local/bin/VoxSlap
echo "VoxSlap installed: VST3 -> ~/.vst3, standalone -> ~/.local/bin/VoxSlap"
