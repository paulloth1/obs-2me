#!/usr/bin/env bash
# Schneller lokaler Multi-M/E-Build (Ninja, ohne volles Xcode). Siehe dev/README.md.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${HERE}/build"

cmake -S "${HERE}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "$@"
cmake --build "${BUILD_DIR}"

echo
echo "✓ multi-me.plugin gebaut + installiert nach:"
echo "  ~/Library/Application Support/obs-studio/plugins/multi-me.plugin"
echo "OBS starten und Log prüfen: [multi-me] Multi-M/E plugin loaded successfully"
