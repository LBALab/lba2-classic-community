#!/usr/bin/env bash
# Build PlayStation Vita homebrew with the digest-pinned VitaSDK image.
# Usage: bash scripts/dev/build-vita.sh [--diagnostics]
# Output: out/vita/ (VPK, eboot.bin, title ID and matching debug ELF)
set -euo pipefail

DIAGNOSTICS=OFF
if [ "$#" -eq 1 ] && [ "$1" = "--diagnostics" ]; then
    DIAGNOSTICS=ON
elif [ "$#" -ne 0 ]; then
    if [ "$#" -eq 1 ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
        sed -n '2,4s/^# //p' "$0"
        exit 0
    fi
    echo "Usage: bash scripts/dev/build-vita.sh [--diagnostics]" >&2
    exit 2
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="$(tr -d '[:space:]' < "$REPO_DIR/docker/vita-image.txt")"
if [[ ! "$IMAGE" =~ ^vitasdk/vitasdk@sha256:[0-9a-f]{64}$ ]]; then
    echo "Invalid image pin in docker/vita-image.txt" >&2
    exit 1
fi
if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    echo "Docker is required and its daemon must be running." >&2
    exit 1
fi
SOURCE_DATE_EPOCH="$(git -C "$REPO_DIR" show -s --format=%ct HEAD)"
mkdir -p "$REPO_DIR/build_logs" "$REPO_DIR/out"
LOG="$REPO_DIR/build_logs/vita-build-$(date +%Y%m%d_%H%M%S).log"
echo "Vita SDK: $IMAGE"
echo "Build log: $LOG"

{
    docker build --build-arg "VITA_SDK_IMAGE=$IMAGE" -t lba2-vita-sdk \
        -f "$REPO_DIR/docker/Dockerfile.vita" "$REPO_DIR/docker"
    docker run --rm \
        --user "$(id -u):$(id -g)" \
        --env SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" \
        --env VITA_DIAGNOSTICS="$DIAGNOSTICS" \
        --env TZ=UTC --env LC_ALL=C \
        --env GIT_CONFIG_COUNT=2 \
        --env GIT_CONFIG_KEY_0=core.trustctime --env GIT_CONFIG_VALUE_0=false \
        --env GIT_CONFIG_KEY_1=core.checkstat --env GIT_CONFIG_VALUE_1=minimal \
        --env HOME=/src/out/vita-home \
        --volume "$REPO_DIR:/src" --workdir /src \
        lba2-vita-sdk bash -euc '
            cmake --preset vita -DLBA2_VITA_DIAGNOSTICS="$VITA_DIAGNOSTICS"
            cmake --build --preset vita --parallel
            cmake --install out/build/vita --prefix /src/out/vita --component vita
            test -s out/vita/lba2cc-vita.vpk
            test -s out/vita/eboot.bin
            test -s out/vita/debug/lba2cc.elf
        '
} 2>&1 | tee "$LOG"
echo "Vita package: $REPO_DIR/out/vita/lba2cc-vita.vpk"
echo "Debug ELF: $REPO_DIR/out/vita/debug/lba2cc.elf"
