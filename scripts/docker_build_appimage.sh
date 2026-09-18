#!/usr/bin/env bash
# ==============================================================================
# Helper script to build edb-next AppImage inside Ubuntu 22.04 LTS Docker container
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="edb-next-builder:ubuntu22.04"

echo "=== Building Docker builder image (${IMAGE_NAME}) ==="
docker build -t "${IMAGE_NAME}" -f "${SCRIPT_DIR}/Dockerfile.appimage" "${SCRIPT_DIR}"

echo "=== Running AppImage build inside container ==="
docker run --rm \
    -v "${ROOT_DIR}:/workspace" \
    --device /dev/fuse --cap-add SYS_ADMIN --security-opt apparmor:unconfined \
    "${IMAGE_NAME}" \
    /bin/bash -c "scripts/build_appimage.sh"

echo "=== Done! AppImage output check ==="
ls -lh "${ROOT_DIR}/build/"*.AppImage || true
