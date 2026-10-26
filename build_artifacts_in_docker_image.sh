#!/bin/bash
# Build script for HF Transport plugin
# Adapted from decomposed-exemplars/build_artifacts_in_docker_image.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${SCRIPT_DIR}/build"
# TODO: add option to specify custom Docker tag and image
DOCKER_TAG="hf-testing"
DOCKER_IMAGE="ghcr.io/tst-race/raceboat/raceboat-plugin-builder"
BUILD_VERSION="${BUILD_VERSION:-1.0.0}"
HOST_UID=$(id -u)
HOST_GID=$(id -g)

# Parse arguments
CLEAN=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -v|--version)
            BUILD_VERSION="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "Building HF Transport plugin version: $BUILD_VERSION"

if [[ "$CLEAN" == "true" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

    echo "Building in Docker container: $DOCKER_IMAGE:$DOCKER_TAG"
    docker run --rm \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/workspace" \
        -w /workspace/build \
        "$DOCKER_IMAGE:$DOCKER_TAG" \
        bash -c "
            cmake .. \
                -DCMAKE_TOOLCHAIN_FILE=/opt/race/race-cmake-modules/toolchains/linux-x86_64.toolchain.cmake \
                -DBUILD_VERSION=$BUILD_VERSION \
                -DNODE_TYPE=linux-x86_64 && \
            make -j\$(nproc)
        "

# Copy artifacts to kit directory
echo "Copying artifacts to kit..."
KIT_BASE_DIR="$SCRIPT_DIR/kit/artifacts"
CLIENT_DIR="$KIT_BASE_DIR/linux-x86_64-client/PluginHFTransport"
SERVER_DIR="$KIT_BASE_DIR/linux-x86_64-server/PluginHFTransport"
mkdir -p "$CLIENT_DIR"
mkdir -p "$SERVER_DIR"

# Copy shared library
if [[ -f "$BUILD_DIR/source/transport/libHFTransport.so" ]]; then
    cp "$BUILD_DIR/source/transport/libHFTransport.so" "$CLIENT_DIR/"
    cp "$BUILD_DIR/source/transport/libHFTransport.so" "$SERVER_DIR/"
    echo "Copied libHFTransport.so"
else
    echo "ERROR: libHFTransport.so not found!"
    exit 1
fi

# Copy manifest
if [[ -f "$SCRIPT_DIR/source/manifest.json" ]]; then
    cp "$SCRIPT_DIR/source/manifest.json" "$CLIENT_DIR/"
    cp "$SCRIPT_DIR/source/manifest.json" "$SERVER_DIR/"
    echo "Copied manifest.json"
else
    echo "ERROR: manifest.json not found!"
    exit 1
fi

# Copy config files (action schedules, etc.)
if [[ -d "$SCRIPT_DIR/config" ]]; then
    mkdir -p "$CLIENT_DIR/config" "$SERVER_DIR/config"
    cp "$SCRIPT_DIR/config/"* "$CLIENT_DIR/config/" 2>/dev/null || true
    cp "$SCRIPT_DIR/config/"* "$SERVER_DIR/config/" 2>/dev/null || true
    echo "Copied config files"
fi

echo "Build complete! Artifacts in: $KIT_BASE_DIR"
ls -la "$CLIENT_DIR/"
ls -la "$SERVER_DIR/"
