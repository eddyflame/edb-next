#!/usr/bin/env bash
# ==============================================================================
# build_keystone.sh - Build a lightweight static libkeystone.a for edb-next
# Builds only the x86/x64 target for maximum speed and minimal footprint (~4.8MB).
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SRC_DIR="/tmp/keystone-src-$$"
BUILD_DIR="/tmp/keystone-build-$$"
DEST_DIR="${PROJECT_ROOT}/third_party/keystone"

echo "=== Cloning Keystone Engine (x86 only) ==="
git clone --depth 1 https://github.com/keystone-engine/keystone.git "${SRC_DIR}"

echo "=== Configuring Keystone (x86 target only) ==="
cmake -B "${BUILD_DIR}" -S "${SRC_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  -DKEYSTONE_BUILD_STATIC=ON \
  -DKEYSTONE_BUILD_SHARED=OFF \
  -DKEYSTONE_X86_SUPPORT=ON \
  -DKEYSTONE_ARM_SUPPORT=OFF \
  -DKEYSTONE_ARM64_SUPPORT=OFF \
  -DKEYSTONE_MIPS_SUPPORT=OFF \
  -DKEYSTONE_PPC_SUPPORT=OFF \
  -DKEYSTONE_SPARC_SUPPORT=OFF \
  -DKEYSTONE_SYSTEMZ_SUPPORT=OFF \
  -DKEYSTONE_HEXAGON_SUPPORT=OFF

echo "=== Compiling Keystone static library ==="
make -C "${BUILD_DIR}" -j"$(nproc)" keystone

echo "=== Installing to ${DEST_DIR} ==="
mkdir -p "${DEST_DIR}/include/keystone" "${DEST_DIR}/lib"
cp -r "${SRC_DIR}/include/keystone/"* "${DEST_DIR}/include/keystone/"
cp "${BUILD_DIR}/llvm/lib/libkeystone.a" "${DEST_DIR}/lib/"

echo "=== Cleaning temporary build directories ==="
rm -rf "${SRC_DIR}" "${BUILD_DIR}"

echo "=== Keystone built and installed successfully to ${DEST_DIR} ==="
