#!/usr/bin/env bash
# Build script for HF Stego Transport plugin
# Runs inside the plugin builder Docker image

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_NAME="hf-stego-transport"

echo "Building ${PLUGIN_NAME} plugin..."

# Detect architecture
ARCH=$(uname -m)
if [[ "$ARCH" == "x86_64" ]]; then
    TARGET_ARCH="x86_64"
elif [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
    TARGET_ARCH="arm64-v8a"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

# Build directory
BUILD_DIR="${SCRIPT_DIR}/build/${TARGET_ARCH}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
cmake "${SCRIPT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/opt/raceboat/plugins/${PLUGIN_NAME}" \
    -DCMAKE_PREFIX_PATH="/linux/${TARGET_ARCH}" \
    -DCMAKE_MODULE_PATH="/linux/${TARGET_ARCH}/lib/cmake"

# Build
make -j$(nproc)

# Install to kit directory
KIT_DIR="${SCRIPT_DIR}/kit/artifacts/linux-${TARGET_ARCH}-server/PluginHFTransport"
mkdir -p "${KIT_DIR}"
make install DESTDIR="${KIT_DIR}"

# Move installed files from nested install path to plugin directory root
# The install puts them in ${KIT_DIR}/opt/raceboat/plugins/hf-stego-transport/
# but raceboat expects them directly in ${KIT_DIR}/
mv "${KIT_DIR}/opt/raceboat/plugins/hf-stego-transport/"* "${KIT_DIR}/" 2>/dev/null || true

# Clean up empty nested directories
rm -rf "${KIT_DIR}/opt"

echo "Build complete. Kit at: ${KIT_DIR}"
echo "Manifest:"
cat "${KIT_DIR}/manifest.json"