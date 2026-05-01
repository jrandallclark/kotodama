#!/bin/bash

# Script to configure, build, and run the Kotodama Qt application

set -e  # Exit on error

echo "=== Kotodama Build & Run ==="
echo ""

# Configure
echo "[1/3] Configuring CMake..."
cmake -B build -S . -DDEV_BUILD=ON

# Build
echo ""
echo "[2/3] Building application..."
cmake --build build

# Run
echo ""
echo "[3/3] Launching application..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    open build/kotodama.app
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    ./build/kotodama
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows
    ./build/Release/kotodama.exe || ./build/Debug/kotodama.exe
else
    echo "Unknown OS type: $OSTYPE"
    echo "Please run the application manually from the build directory"
    exit 1
fi

echo ""
echo "=== Done ==="
