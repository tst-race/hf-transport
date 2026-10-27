#!/usr/bin/env bash
# Builds the unit tests in the raceboat-plugin-builder image and runs them in the raceboat-runtime
# image (the builder's libraceSdkCommon.so needs a newer glibc than the builder ships).
#
#   ./run_unit_tests.sh [--image-tag TAG] [gtest args, e.g. --gtest_filter=LinkMap*]
# TAG defaults to $RACEBOAT_IMAGE_TAG, else 'hf-testing'.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TAG="${RACEBOAT_IMAGE_TAG:-hf-testing}"

# Leading --image-tag/-t TAG; everything after is passed to the gtest binary.
while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--image-tag) [[ $# -ge 2 ]] || { echo "$1 requires a value" >&2; exit 1; }; TAG="$2"; shift 2 ;;
        --image-tag=*) TAG="${1#*=}"; shift ;;
        *) break ;;
    esac
done

"${SCRIPT_DIR}/build_artifacts_in_docker_image.sh" --image-tag "${TAG}" --unit-tests

docker run --rm -v "${SCRIPT_DIR}":/workspace -w /workspace \
    "ghcr.io/tst-race/raceboat/raceboat-runtime:${TAG}" \
    /workspace/build/test/source/hf_transport_tests "$@"
