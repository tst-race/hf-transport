#!/usr/bin/env bash
# Build test script for HF Stego Transport plugin

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$(dirname "$SCRIPT_DIR")"

echo "Building HF Stego Transport plugin..."

# Check for raceboat SDK
if [ ! -d "/opt/raceboat/include" ]; then
    echo "Error: raceboat SDK not found at /opt/raceboat"
    echo "Please build raceboat first or mount the SDK"
    exit 1
fi

# Create build directory
BUILD_DIR="$PLUGIN_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
cmake "$PLUGIN_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/opt/raceboat/plugins/hf-stego-transport" \
    2>&1 | tee cmake.log

# Build
echo "Building..."
make -j$(nproc) 2>&1 | tee build.log

# Check if libraries were built
if [ -f "libPluginHFTransport.so" ]; then
    echo "Build successful!"
    echo "Libraries:"
    ls -la libPluginHFTransport.so
else
    echo "Build failed - libraries not found"
    exit 1
fi

# Create kit directory
KIT_DIR="$PLUGIN_DIR/kit/artifacts/linux-x86_64-server/PluginHFTransport"
mkdir -p "$KIT_DIR"

# Copy libraries
cp libPluginHFTransport.so "$KIT_DIR/"

# Copy manifest
cp "$PLUGIN_DIR/source/manifest.json" "$KIT_DIR/"

echo "Kit created at: $KIT_DIR"
echo "Manifest:"
cat "$KIT_DIR/manifest.json"