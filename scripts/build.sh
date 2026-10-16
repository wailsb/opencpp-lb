#!/bin/bash
# Usage: scripts/build.sh [preset]   (default: dev; see CMakePresets.json)

# Exit immediately if a command exits with a non-zero status
set -e
cd "$(dirname "$0")/.."

PRESET="${1:-dev}"

echo "[INFO] Configuring (preset: $PRESET)..."
cmake --preset "$PRESET"

echo "[INFO] Compiling project..."
cmake --build --preset "$PRESET"

echo "[SUCCESS] Build complete! Executable is located at ./build/$PRESET/src/opencpp_lb"
