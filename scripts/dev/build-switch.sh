#!/usr/bin/env bash
# Build Nintendo Switch homebrew with the digest-pinned devkitPro image.
# Usage: bash scripts/dev/build-switch.sh
# Output: out/switch/ (SD-card tree and debug ELF), out/lba2cc-switch.zip
set -euo pipefail

if [ "$#" -ne 0 ]; then
    if [ "$#" -eq 1 ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
        sed -n '2,4s/^# //p' "$0"
        exit 0
    fi
    echo "Usage: bash scripts/dev/build-switch.sh" >&2
    exit 2
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="$(tr -d '[:space:]' < "$REPO_DIR/docker/switch-image.txt")"
if [[ ! "$IMAGE" =~ ^devkitpro/devkita64@sha256:[0-9a-f]{64}$ ]]; then
    echo "Invalid image pin in docker/switch-image.txt" >&2
    exit 1
fi
if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    echo "Docker is required and its daemon must be running." >&2
    exit 1
fi
SOURCE_DATE_EPOCH="$(git -C "$REPO_DIR" show -s --format=%ct HEAD)"
mkdir -p "$REPO_DIR/build_logs" "$REPO_DIR/out"
LOG="$REPO_DIR/build_logs/switch-build-$(date +%Y%m%d_%H%M%S).log"
echo "Switch SDK: $IMAGE"
echo "Build log: $LOG"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" \
    --env TZ=UTC --env LC_ALL=C \
    --env HOME=/src/out/switch-home \
    --volume "$REPO_DIR:/src" --workdir /src \
    "$IMAGE" bash -euc '
        cmake --preset switch
        cmake --build --preset switch --parallel
        cmake --install out/build/switch --prefix /src/out/switch --component switch
        cd out/switch
        test -s switch/lba2cc/lba2cc.nro
        test -s debug/lba2cc.elf
        touch -d "@$SOURCE_DATE_EPOCH" switch/lba2cc/lba2cc.nro \
            switch/lba2cc/README.txt switch/lba2cc/LICENSE
        cmake -E rm -f ../lba2cc-switch.zip
        zip -X -q ../lba2cc-switch.zip switch/lba2cc/lba2cc.nro \
            switch/lba2cc/README.txt switch/lba2cc/LICENSE
    ' 2>&1 | tee "$LOG"
echo "SD-card package: $REPO_DIR/out/lba2cc-switch.zip"
echo "Debug ELF: $REPO_DIR/out/switch/debug/lba2cc.elf"
