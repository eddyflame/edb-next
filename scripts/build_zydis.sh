#!/usr/bin/env bash
# ==============================================================================
# build_zydis.sh - Build a lightweight static libzydis.a for edb-next
# Builds Zydis and Zycore for x86/x64, producing a unified static library (~926KB).
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SRC_DIR="/tmp/zydis-src-$$"
BUILD_DIR="/tmp/zydis-build-$$"
INSTALL_DIR="/tmp/zydis-install-$$"
DEST_DIR="${PROJECT_ROOT}/third_party/zydis"

echo "=== Cloning Zydis with Zycore submodule ==="
git clone --recursive --depth 1 https://github.com/zyantific/zydis.git "${SRC_DIR}"

echo "=== Configuring Zydis (Release, Static Library) ==="
cmake -B "${BUILD_DIR}" -S "${SRC_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DZYDIS_BUILD_SHARED_LIB=OFF \
  -DZYDIS_BUILD_EXAMPLES=OFF \
  -DZYDIS_BUILD_TOOLS=OFF \
  -DZYCORE_BUILD_SHARED_LIB=OFF \
  -DZYCORE_BUILD_TESTS=OFF \
  -DZYDIS_BUILD_TESTS=OFF

echo "=== Compiling Zydis & Zycore ==="
make -C "${BUILD_DIR}" -j"$(nproc)"

echo "=== Installing to temporary prefix ==="
cmake --install "${BUILD_DIR}" --prefix "${INSTALL_DIR}"
cmake --install "${BUILD_DIR}/zycore" --prefix "${INSTALL_DIR}"

echo "=== Creating unified static libzydis.a ==="
mkdir -p "${DEST_DIR}/include" "${DEST_DIR}/lib"
cd "${INSTALL_DIR}/lib"
printf "create libzydis.a\naddlib libZydis.a\naddlib libZycore.a\nsave\nend" | ar -M

echo "=== Copying headers and library to ${DEST_DIR} ==="
cp -r "${INSTALL_DIR}/include/Zydis" "${DEST_DIR}/include/"
cp -r "${INSTALL_DIR}/include/Zycore" "${DEST_DIR}/include/"
cp "${INSTALL_DIR}/lib/libzydis.a" "${DEST_DIR}/lib/"

echo "=== Cleaning temporary build directories ==="
rm -rf "${SRC_DIR}" "${BUILD_DIR}" "${INSTALL_DIR}"

echo "=== Zydis built and installed successfully to ${DEST_DIR} ==="
