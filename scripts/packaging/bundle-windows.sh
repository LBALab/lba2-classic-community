#!/usr/bin/env bash
# Bundle a built Windows lba2cc.exe into a portable release ZIP.
#
# Shared by the local dry-run script (scripts/dev/build-windows-release.sh)
# and the eventual CI release workflow (.github/workflows/release-windows.yml).
# Three callers, one packaging codepath — keeps ZIP layout from drifting.
#
# Usage:
#   bundle-windows.sh --exe <path/to/lba2cc.exe> \
#                     --version <version-string> \
#                     --arch <i686|x64> \
#                     --build-dir <cmake-build-dir> \
#                     --output-dir <where-to-drop-the-zip>
#
# Produces: <output-dir>/lba2cc-<version>-windows-<arch>.zip
#
# ZIP layout:
#   lba2cc-<version>-windows-<arch>/
#       lba2cc.exe          (or whatever LBA2_EXECUTABLE_NAME resolved to;
#                            picked up from the input file's basename)
#       README.txt          (CRLF line endings; populated from
#                            scripts/packaging/windows-readme.txt.in)
#       LICENSE.txt         (GPL-2.0 from repo root)
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
        echo "bundle-windows.sh: missing required arg: ${var,,}" >&2
        exit 2
    fi
done

if [[ ! -f "$EXE_PATH" ]]; then
    echo "bundle-windows.sh: exe not found at $EXE_PATH" >&2
    exit 1
fi

# Derive REPO_ROOT from the script's own path rather than `git rev-parse`
# so this script works in environments without git on PATH (the MSYS2
# shell on GitHub-hosted Windows runners is one — git is host-side, not
# inside the MSYS2 prefix). The script lives at
# scripts/packaging/bundle-windows.sh, so two `..` reach the repo root.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXE_NAME="$(basename "$EXE_PATH")"           # lba2cc.exe (or override)
EXE_STEM="${EXE_NAME%.exe}"                  # lba2cc
ARTIFACT_NAME="${EXE_STEM}-${VERSION}-windows-${ARCH}"
ARTIFACT_DIR="${OUTPUT_DIR}/${ARTIFACT_NAME}"
ARTIFACT_ZIP="${OUTPUT_DIR}/${ARTIFACT_NAME}.zip"

echo "[bundle-windows] exe:        $EXE_PATH"
echo "[bundle-windows] version:    $VERSION"
echo "[bundle-windows] arch:       $ARCH"
echo "[bundle-windows] artifact:   $ARTIFACT_ZIP"

# Fresh staging dir.
rm -rf "$ARTIFACT_DIR" "$ARTIFACT_ZIP"
mkdir -p "$ARTIFACT_DIR"

# 1. Binary.
cp "$EXE_PATH" "$ARTIFACT_DIR/$EXE_NAME"

# 2. README.txt — substitute LBA2_* values from the build's CMakeCache,
#    then convert to CRLF for Windows notepad-friendliness.
PRODUCT_NAME=$(awk -F= '/^LBA2_PRODUCT_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "LBA2 Classic Community")
PRODUCT_DESC=$(awk -F= '/^LBA2_PRODUCT_DESCRIPTION:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "Community fork")

sed -e "s|@LBA2_PRODUCT_NAME@|${PRODUCT_NAME}|g" \
    -e "s|@LBA2_PRODUCT_DESCRIPTION@|${PRODUCT_DESC}|g" \
    -e "s|@LBA2_VERSION_STRING@|${VERSION}|g" \
    -e "s|@LBA2_EXECUTABLE_NAME@|${EXE_STEM}|g" \
    "$REPO_ROOT/scripts/packaging/windows-readme.txt.in" \
    | sed 's/$/\r/' > "$ARTIFACT_DIR/README.txt"

# 3. LICENSE.txt — copy from repo root with CRLF line endings.
sed 's/$/\r/' "$REPO_ROOT/LICENSE" > "$ARTIFACT_DIR/LICENSE.txt"

# 4. Zip it up. Use the parent dir as ZIP base so the inner top-level
#    folder matches ARTIFACT_NAME (so users unzipping into Downloads
#    don't dump three loose files into the directory).
#
#    Prefer `zip`; fall back to python3's zipfile module if not present
#    (some minimal dev environments have python but not zip; CI runners
#    have both).
ZIP_BASENAME="$(basename "$ARTIFACT_ZIP")"
if command -v zip >/dev/null 2>&1; then
    ( cd "$OUTPUT_DIR" && zip -qr "$ZIP_BASENAME" "$ARTIFACT_NAME" )
elif command -v python3 >/dev/null 2>&1; then
    ( cd "$OUTPUT_DIR" && python3 -c "
import os, sys, zipfile
with zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(sys.argv[2]):
        for f in files:
            p = os.path.join(root, f)
            z.write(p, os.path.relpath(p, '.'))
" "$ZIP_BASENAME" "$ARTIFACT_NAME" )
else
    echo "[bundle-windows] need 'zip' or 'python3' to create the archive" >&2
    exit 1
fi

# 5. Optional: confirm the binary is genuinely standalone (no MSYS2 DLL
#    dependencies). Best-effort — only if objdump understands the file.
if command -v objdump >/dev/null 2>&1; then
    NON_SYSTEM=$(objdump -p "$EXE_PATH" 2>/dev/null \
        | grep -i "DLL Name" \
        | grep -ivE "(KERNEL32|USER32|GDI32|WINMM|ADVAPI32|MSVCRT|SHELL32|OLE32|OLEAUT32|WS2_32|IMM32|VERSION|UXTHEME|DWMAPI|WINSPOOL|COMDLG32|COMCTL32|DDRAW|SETUPAPI|HID|CFGMGR32|DXGI|D3D9|OPENGL32)\.DLL" \
        || true)
    if [[ -n "$NON_SYSTEM" ]]; then
        echo "[bundle-windows] WARN: non-system DLL dependencies remain:" >&2
        echo "$NON_SYSTEM" | sed 's/^/  /' >&2
        echo "[bundle-windows] (Build with -DLBA2_LINK_STATIC=ON to fold these in.)" >&2
    else
        echo "[bundle-windows] DLL audit: only system DLLs — single-exe ✓"
    fi
fi

# 6. Final report.
ZIP_SIZE=$(du -h "$ARTIFACT_ZIP" | cut -f1)
echo "[bundle-windows] done: $ARTIFACT_ZIP ($ZIP_SIZE)"
