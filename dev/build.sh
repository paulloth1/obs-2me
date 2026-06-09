#!/usr/bin/env bash
# Schneller lokaler 2ME-Build (Ninja, ohne volles Xcode). Siehe dev/README.md.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${HERE}/build"

cmake -S "${HERE}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "$@"
cmake --build "${BUILD_DIR}"

echo
echo "✓ obs-2me.plugin gebaut + installiert nach:"
echo "  ~/Library/Application Support/obs-studio/plugins/obs-2me.plugin"
echo "OBS starten und Log prüfen: [obs-2me] 2ME plugin loaded successfully"
