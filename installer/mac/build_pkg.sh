#!/bin/bash
# Builds VoxSlap-macOS.pkg from the Release artefacts.
# Usage: installer/mac/build_pkg.sh <artefacts Release dir> <output dir> <version>
set -euo pipefail
ART="$1"; OUT="$2"; VERSION="${3:-1.0.0}"
WORK="$(mktemp -d)"
ROOT="$WORK/root"
export COPYFILE_DISABLE=1

mkdir -p "$ROOT/Library/Audio/Plug-Ins/Components" "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Applications"
cp -R "$ART/AU/VoxSlap.component" "$ROOT/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/VoxSlap.vst3" "$ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/Standalone/VoxSlap.app" "$ROOT/Applications/"

xattr -cr "$ROOT"

# Stop the installer from "relocating" bundles to wherever an older copy lives.
pkgbuild --analyze --root "$ROOT" "$WORK/components.plist"
plutil -replace BundleIsRelocatable -bool NO "$WORK/components.plist" 2>/dev/null || true
for i in $(seq 0 10); do
    plutil -replace "$i.BundleIsRelocatable" -bool NO "$WORK/components.plist" 2>/dev/null || break
done

mkdir -p "$OUT"
pkgbuild --root "$ROOT" --component-plist "$WORK/components.plist" \
         --identifier com.homebrewaudio.voxslap --version "$VERSION" \
         --install-location / "$OUT/VoxSlap-macOS-Installer.pkg"
rm -rf "$WORK"
echo "Built $OUT/VoxSlap-macOS-Installer.pkg"
