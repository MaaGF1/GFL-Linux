#!/bin/bash
# Stop on error
set -e

echo "========================================="
echo "[*] Building GFL Linux Native Loader"
echo "========================================="

# Get current script directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$DIR/build"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake and Make
cmake ..
make -j$(nproc)

echo ""
echo "========================================="
echo "[+] Build complete!"
echo "[*] Executable: $BUILD_DIR/bin/gfl_loader"
echo ""
echo "[*] Quick Test Command:"
echo "    $BUILD_DIR/bin/gfl_loader $DIR/src/UnityPlayer.dll"
echo "========================================="