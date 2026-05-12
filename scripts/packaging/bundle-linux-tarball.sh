#!/usr/bin/env bash
# Bundle a built Linux lba2cc binary into a portable release tarball.
#
# Shared by the local dry-run script (scripts/dev/build-linux-tarball.sh)
# and the CI release workflow (.github/workflows/release-linux-tarball.yml).
# Mirrors the shape of scripts/packaging/bundle-windows.sh so the tarball
# layout cannot drift from the Windows ZIP layout — both are "drop the
# binary anywhere and run" portable distributions.
#
# This is the static-binary alternative to the AppImage release: same
# binary contents, no AppImage runtime, no desktop integration. Users who
# want a menu entry / icon should grab the AppImage instead.
#
# Usage:
#   bundle-linux-tarball.sh --exe <path/to/lba2cc> \
#                           --version <version-string> \
#                           --arch <x86_64|aarch64> \
#                           --build-dir <cmake-build-dir> \
#                           --output-dir <where-to-drop-the-tarball>
#
# Produces: <output-dir>/lba2cc-<version>-linux-<arch>.tar.gz
#
# Tarball layout:
#   lba2cc-<version>-linux-<arch>/
#       lba2cc          (or whatever LBA2_EXECUTABLE_NAME resolved to;
#                        picked up from the input file's basename)
#       README.txt      (LF line endings; populated from
#                        scripts/packaging/linux-readme.txt.in)
#       LICENSE.txt     (GPL-2.0 from repo root)
set -euo pipefail

EXE_PATH=""
VERSION=""
ARCH=""
BUILD_DIR=""
OUTPUT_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --exe) EXE_PATH="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --arch) ARCH="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            sed -n '/^# Usage:/,/^set -e/p' "$0" | sed 's/^# \?//' | head -n -1
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

for var in EXE_PATH VERSION ARCH BUILD_DIR OUTPUT_DIR; do
    if [[ -z "${!var}" ]]; then
        echo "bundle-linux-tarball.sh: missing required arg: ${var,,}" >&2
        exit 2
    fi
done

if [[ ! -f "$EXE_PATH" ]]; then
    echo "bundle-linux-tarball.sh: binary not found at $EXE_PATH" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXE_NAME="$(basename "$EXE_PATH")"           # lba2cc (or override)
EXE_STEM="$EXE_NAME"                         # no extension on Linux
ARTIFACT_NAME="${EXE_STEM}-${VERSION}-linux-${ARCH}"
ARTIFACT_DIR="${OUTPUT_DIR}/${ARTIFACT_NAME}"
ARTIFACT_TGZ="${OUTPUT_DIR}/${ARTIFACT_NAME}.tar.gz"

echo "[bundle-linux-tarball] exe:        $EXE_PATH"
echo "[bundle-linux-tarball] version:    $VERSION"
echo "[bundle-linux-tarball] arch:       $ARCH"
echo "[bundle-linux-tarball] artifact:   $ARTIFACT_TGZ"

# Fresh staging dir.
rm -rf "$ARTIFACT_DIR" "$ARTIFACT_TGZ"
mkdir -p "$ARTIFACT_DIR"

# 1. Binary — preserve executable bit.
install -m 0755 "$EXE_PATH" "$ARTIFACT_DIR/$EXE_NAME"

# 2. README.txt — substitute LBA2_* values from the build's CMakeCache.
#    LF line endings (Linux); no CRLF conversion.
PRODUCT_NAME=$(awk -F= '/^LBA2_PRODUCT_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "LBA2 Classic Community")
PRODUCT_DESC=$(awk -F= '/^LBA2_PRODUCT_DESCRIPTION:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "Community fork")

sed -e "s|@LBA2_PRODUCT_NAME@|${PRODUCT_NAME}|g" \
    -e "s|@LBA2_PRODUCT_DESCRIPTION@|${PRODUCT_DESC}|g" \
    -e "s|@LBA2_VERSION_STRING@|${VERSION}|g" \
    -e "s|@LBA2_EXECUTABLE_NAME@|${EXE_STEM}|g" \
    "$REPO_ROOT/scripts/packaging/linux-readme.txt.in" \
    > "$ARTIFACT_DIR/README.txt"

# 3. LICENSE.txt — copy from repo root as-is (LF on Linux).
cp "$REPO_ROOT/LICENSE" "$ARTIFACT_DIR/LICENSE.txt"

# 4. Tar + gzip. Use the parent dir as base so the inner top-level folder
#    matches ARTIFACT_NAME (mirrors the Windows ZIP layout).
( cd "$OUTPUT_DIR" && tar czf "$(basename "$ARTIFACT_TGZ")" "$ARTIFACT_NAME" )

# 5. Audit shared-library dependencies. Parallels the objdump DLL audit in
#    bundle-windows.sh — anything outside the glibc/loader/libstdc++ core
#    is a sign static linking didn't take, and the tarball will fail on
#    machines without that library installed.
if command -v ldd >/dev/null 2>&1; then
    # Whitelist: vDSO, dynamic loader, glibc family, libstdc++, libgcc_s.
    # libdl/librt/libpthread are part of glibc and present on every system.
    # libm is part of glibc as well. Anything else (libSDL3*, libsmacker,
    # libGL, libwayland-*, libX11, etc.) is a real dependency the user
    # would need installed, which defeats the point of the static tarball.
    NON_SYSTEM=$(ldd "$EXE_PATH" 2>/dev/null \
        | awk '{print $1}' \
        | grep -ivE "^(linux-vdso\.so|linux-gate\.so|/lib(64)?/ld-linux|ld-linux|libc\.so|libm\.so|libdl\.so|librt\.so|libpthread\.so|libstdc\+\+\.so|libgcc_s\.so|libresolv\.so)" \
        | grep -vE "^$" \
        || true)
    if [[ -n "$NON_SYSTEM" ]]; then
        echo "[bundle-linux-tarball] WARN: non-system shared library dependencies remain:" >&2
        echo "$NON_SYSTEM" | sed 's/^/  /' >&2
        echo "[bundle-linux-tarball] (Build with -DLBA2_LINK_STATIC=ON to fold these in.)" >&2
    else
        echo "[bundle-linux-tarball] ldd audit: only system libraries — portable ✓"
    fi
fi

# 6. Final report.
TGZ_SIZE=$(du -h "$ARTIFACT_TGZ" | cut -f1)
echo "[bundle-linux-tarball] done: $ARTIFACT_TGZ ($TGZ_SIZE)"
