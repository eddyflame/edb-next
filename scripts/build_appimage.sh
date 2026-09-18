#!/usr/bin/env bash
# ==============================================================================
# edb-next: Linux AppImage Packaging Script
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TOOLS_DIR="${ROOT_DIR}/tools"
APPDIR="${BUILD_DIR}/AppDir"
OUTPUT_APPIMAGE="${BUILD_DIR}/edb-next-x86_64.AppImage"

echo "=== [1/5] Initializing Environment & Directories ==="
mkdir -p "${TOOLS_DIR}"
mkdir -p "${BUILD_DIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}"

# Determine qmake for Qt6
if command -v qmake6 &>/dev/null; then
    export QMAKE="$(command -v qmake6)"
elif [ -x "/usr/lib/qt6/bin/qmake" ]; then
    export QMAKE="/usr/lib/qt6/bin/qmake"
elif command -v qmake &>/dev/null; then
    export QMAKE="$(command -v qmake)"
else
    echo "ERROR: Qt6 qmake not found! Please install qt6-base-dev / qt6-tools-dev." >&2
    exit 1
fi
echo "Using QMAKE: ${QMAKE}"

# Determine if AppImage requires --appimage-extract-and-run (e.g. no /dev/fuse in Docker)
APPIMAGE_EXTRACT_ARG=""
if ! command -v fusermount &>/dev/null && ! command -v fusermount3 &>/dev/null; then
    APPIMAGE_EXTRACT_ARG="--appimage-extract-and-run"
elif [ ! -c /dev/fuse ]; then
    APPIMAGE_EXTRACT_ARG="--appimage-extract-and-run"
fi

# Download linuxdeploy and linuxdeploy-plugin-qt if not already cached
LINUXDEPLOY_BIN="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_BIN="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

echo "=== [2/5] Preparing linuxdeploy Packaging Tools ==="
if [ ! -f "${LINUXDEPLOY_BIN}" ]; then
    echo "Downloading linuxdeploy..."
    wget -q --show-progress -O "${LINUXDEPLOY_BIN}" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "${LINUXDEPLOY_BIN}"
fi

if [ ! -f "${LINUXDEPLOY_QT_BIN}" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -q --show-progress -O "${LINUXDEPLOY_QT_BIN}" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "${LINUXDEPLOY_QT_BIN}"
fi

# Export PATH so linuxdeploy finds its plugins
export PATH="${TOOLS_DIR}:${PATH}"

echo "=== [3/5] Compiling and Installing to AppDir ==="
cmake -B "${BUILD_DIR}" \
    -S "${ROOT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "${BUILD_DIR}" --target edb_next sample_plugin -j"$(nproc)"

# Install into staging AppDir
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

echo "=== [4/5] Running linuxdeploy with Qt6 Plugin ==="
# Exclude certain glibc internal libraries that must be provided by host
export NO_STRIP=true # keep symbols or unstripped as appropriate

# Support both XCB, Wayland, and headless Offscreen in AppImage
export EXTRA_QT_PLUGINS="platforms/libqwayland-generic.so,platforms/libqwayland-egl.so,platforms/libqoffscreen.so"

# Set AppImage runtime environment flags
# Some systems don't have FUSE mounted in build environments
if [ -n "${APPIMAGE_EXTRACT_ARG}" ]; then
    export APPIMAGE_EXTRACT_AND_RUN=1
fi

# Run linuxdeploy to deploy Qt dependencies
"${LINUXDEPLOY_BIN}" ${APPIMAGE_EXTRACT_ARG} \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/usr/share/applications/edb-next.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/edb-next.png" \
    --plugin qt

# Deploy extra platform and wayland integration plugins
QT_PLUGINS_DIR="$(${QMAKE} -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
if [ -d "${QT_PLUGINS_DIR}" ]; then
    # 1. Platforms (offscreen & wayland)
    mkdir -p "${APPDIR}/usr/plugins/platforms"
    for extra_plat in libqoffscreen.so libqwayland-generic.so libqwayland-egl.so; do
        if [ -f "${QT_PLUGINS_DIR}/platforms/${extra_plat}" ]; then
            cp -f "${QT_PLUGINS_DIR}/platforms/${extra_plat}" "${APPDIR}/usr/plugins/platforms/"
            if command -v patchelf &>/dev/null; then
                patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "${APPDIR}/usr/plugins/platforms/${extra_plat}" 2>/dev/null || true
            fi
        fi
    done

    # 2. Wayland shell integration (CRITICAL: without this, Qt Wayland hangs and cannot show windows!)
    if [ -d "${QT_PLUGINS_DIR}/wayland-shell-integration" ]; then
        mkdir -p "${APPDIR}/usr/plugins/wayland-shell-integration"
        cp -f "${QT_PLUGINS_DIR}/wayland-shell-integration/"*.so "${APPDIR}/usr/plugins/wayland-shell-integration/" 2>/dev/null || true
        for f in "${APPDIR}/usr/plugins/wayland-shell-integration/"*.so; do
            [ -f "$f" ] && command -v patchelf &>/dev/null && patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "$f" 2>/dev/null || true
        done
    fi

    # 3. Wayland graphics integration
    if [ -d "${QT_PLUGINS_DIR}/wayland-graphics-integration-client" ]; then
        mkdir -p "${APPDIR}/usr/plugins/wayland-graphics-integration-client"
        cp -f "${QT_PLUGINS_DIR}/wayland-graphics-integration-client/"*.so "${APPDIR}/usr/plugins/wayland-graphics-integration-client/" 2>/dev/null || true
        for f in "${APPDIR}/usr/plugins/wayland-graphics-integration-client/"*.so; do
            [ -f "$f" ] && command -v patchelf &>/dev/null && patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "$f" 2>/dev/null || true
        done
    fi

    # 4. Wayland decorations
    if [ -d "${QT_PLUGINS_DIR}/wayland-decoration-client" ]; then
        mkdir -p "${APPDIR}/usr/plugins/wayland-decoration-client"
        cp -f "${QT_PLUGINS_DIR}/wayland-decoration-client/"*.so "${APPDIR}/usr/plugins/wayland-decoration-client/" 2>/dev/null || true
        for f in "${APPDIR}/usr/plugins/wayland-decoration-client/"*.so; do
            [ -f "$f" ] && command -v patchelf &>/dev/null && patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "$f" 2>/dev/null || true
        done
    fi
fi

# Create robust AppRun launcher script (replaces simple symlink)
rm -f "${APPDIR}/AppRun"
cat << 'EOF' > "${APPDIR}/AppRun"
#!/usr/bin/env bash
# ==============================================================================
# edb-next: AppRun Wrapper Script
# ==============================================================================
set -e

HERE="$(dirname "$(readlink -f "${0}")")"
export APPDIR="${HERE}"
export PATH="${HERE}/usr/bin:${PATH}"

# Library paths
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Qt environment
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"

# Fallback: Prefer wayland with xcb fallback if running in Wayland environment
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        export QT_QPA_PLATFORM="wayland;xcb"
    fi
fi

exec "${HERE}/usr/bin/edb_next" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# Output AppImage
"${LINUXDEPLOY_BIN}" ${APPIMAGE_EXTRACT_ARG} \
    --appdir "${APPDIR}" \
    --output appimage



# Find generated AppImage in ROOT_DIR or current directory and move to BUILD_DIR
GENERATED_FILE=$(find "${ROOT_DIR}" -maxdepth 1 -name "EDB_Next*.AppImage" -o -name "edb*.AppImage" | head -n 1)
if [ -n "${GENERATED_FILE}" ] && [ -f "${GENERATED_FILE}" ]; then
    mv "${GENERATED_FILE}" "${OUTPUT_APPIMAGE}"
fi

echo "=== [5/5] Packaging Completed Successfully! ==="
if [ -f "${OUTPUT_APPIMAGE}" ]; then
    ls -lh "${OUTPUT_APPIMAGE}"
    echo "Target AppImage generated at: ${OUTPUT_APPIMAGE}"
else
    # Check if linuxdeploy placed it in current workdir
    echo "Checking output files:"
    find "${ROOT_DIR}" -maxdepth 2 -name "*.AppImage"
fi
