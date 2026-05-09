#!/usr/bin/env bash
# Bundle a built lba2cc.app into a macOS DMG release artifact.
#
# Shared by the local dry-run script (scripts/dev/build-macos-release.sh)
# and the CI release workflow (.github/workflows/release-macos.yml). One
# packaging codepath; same shape as bundle-windows.sh.
#
# Usage:
#   bundle-macos.sh --app <path/to/lba2cc.app> \
#                   --version <version-string> \
#                   --arch <arm64|x86_64> \
#                   --build-dir <cmake-build-dir> \
#                   --output-dir <where-to-drop-the-dmg>
#
# Produces: <output-dir>/lba2cc-<version>-macos-<arch>.dmg
#
# DMG layout (mounted view):
#   <product-name>-<version>/
#       lba2cc.app
#       Applications -> /Applications  (drag-to-install symlink)
#       README.txt   (CRLF-free; macOS notepad-friendly with LF)
#
# Requires macOS host (uses hdiutil; no cross-platform alternative that
# produces a real DMG). Use scripts/dev/build-macos-release.sh as the
# entry point for local iteration.
set -euo pipefail

APP_PATH=""
VERSION=""
ARCH=""
BUILD_DIR=""
OUTPUT_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --app) APP_PATH="$2"; shift 2 ;;
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

for var in APP_PATH VERSION ARCH BUILD_DIR OUTPUT_DIR; do
    if [[ -z "${!var}" ]]; then
        echo "bundle-macos.sh: missing required arg: ${var,,}" >&2
        exit 2
    fi
done

if [[ ! -d "$APP_PATH" ]]; then
    echo "bundle-macos.sh: .app not found at $APP_PATH" >&2
    exit 1
fi

if ! command -v hdiutil >/dev/null 2>&1; then
    echo "bundle-macos.sh: hdiutil not on PATH — must run on a macOS host" >&2
    exit 1
fi

# Same path-derivation pattern as bundle-windows.sh (no git dependency).
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_NAME="$(basename "$APP_PATH")"          # lba2cc.app (or override)
APP_STEM="${APP_NAME%.app}"                 # lba2cc
ARTIFACT_NAME="${APP_STEM}-${VERSION}-macos-${ARCH}"
ARTIFACT_DMG="${OUTPUT_DIR}/${ARTIFACT_NAME}.dmg"
STAGING_DIR="${OUTPUT_DIR}/${ARTIFACT_NAME}-staging"

echo "[bundle-macos] app:        $APP_PATH"
echo "[bundle-macos] version:    $VERSION"
echo "[bundle-macos] arch:       $ARCH"
echo "[bundle-macos] artifact:   $ARTIFACT_DMG"

# Fresh staging directory.
rm -rf "$STAGING_DIR" "$ARTIFACT_DMG"
mkdir -p "$STAGING_DIR"

# 1. Copy .app — preserve symlinks, perms, code signatures, etc. Use cp -R
#    since /usr/bin/cp on macOS preserves bundle metadata reliably (rsync
#    can also work but cp -R is the established convention).
cp -R "$APP_PATH" "$STAGING_DIR/$APP_NAME"

# 2. Drag-to-install hint: a relative symlink to /Applications.
ln -s /Applications "$STAGING_DIR/Applications"

# 3. README — substitute LBA2_* values from CMakeCache, LF line endings
#    (macOS TextEdit handles LF cleanly, unlike Windows' notepad).
PRODUCT_NAME=$(awk -F= '/^LBA2_PRODUCT_NAME:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "LBA2 Classic Community")
PRODUCT_DESC=$(awk -F= '/^LBA2_PRODUCT_DESCRIPTION:[^=]+=/{print $2; exit}' \
    "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "Community fork")

sed -e "s|@LBA2_PRODUCT_NAME@|${PRODUCT_NAME}|g" \
    -e "s|@LBA2_PRODUCT_DESCRIPTION@|${PRODUCT_DESC}|g" \
    -e "s|@LBA2_VERSION_STRING@|${VERSION}|g" \
    -e "s|@LBA2_EXECUTABLE_NAME@|${APP_STEM}|g" \
    "$REPO_ROOT/scripts/packaging/macos-readme.txt.in" \
    > "$STAGING_DIR/README.txt"

# 4. LICENSE — copy from repo root into the DMG window so users see it
#    alongside the .app. macOS users don't expect to dig into the bundle's
#    Contents/Resources for the license.
cp "$REPO_ROOT/LICENSE" "$STAGING_DIR/LICENSE.txt"

# 5. Build the DMG. Use UDZO (zlib-compressed) for size; -srcfolder packs
#    the entire staging dir; -volname is what shows in Finder when mounted.
hdiutil create \
    -volname "${PRODUCT_NAME} ${VERSION}" \
    -srcfolder "$STAGING_DIR" \
    -ov \
    -format UDZO \
    "$ARTIFACT_DMG" >/dev/null

# 6. Cleanup staging dir.
rm -rf "$STAGING_DIR"

# 7. Final report.
DMG_SIZE=$(du -h "$ARTIFACT_DMG" | cut -f1)
echo "[bundle-macos] done: $ARTIFACT_DMG ($DMG_SIZE)"
