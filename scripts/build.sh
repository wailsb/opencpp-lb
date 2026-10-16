#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "[INFO] Creating build directory..."
mkdir -p build
cd build

echo "[INFO] Running CMake..."
cmake ..

echo "[INFO] Compiling project..."
cmake --build . -j"$(nproc)"

echo "[SUCCESS] Build complete! Executable is located at ./build/opencpp_lb"