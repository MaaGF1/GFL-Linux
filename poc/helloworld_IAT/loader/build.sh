#!/bin/bash
# Stop on error
set -e

echo "========================================="
echo "[*] Building Linux Custom PE Loader"
echo "========================================="

# Get current script directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$DIR/build"

# Clean and create build dir
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake and Make
cmake ..
make -j$(nproc)

echo "========================================="
echo "[+] Build complete!"
echo "[*] Exe located at: $BUILD_DIR/bin/gfl_loader"
echo ""
echo "[*] Quick Test Command:"
echo "    ./build/bin/gfl_loader ../dll/build/bin/helloworld.dll"
echo "========================================="