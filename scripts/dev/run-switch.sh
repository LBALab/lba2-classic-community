#!/usr/bin/env bash
# Upload and launch the built Switch NRO through Homebrew Menu's netloader.
# Usage: bash scripts/dev/run-switch.sh <switch-ip>
set -euo pipefail

if [ "$#" -eq 1 ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
    sed -n '2,3s/^# //p' "$0"
    exit 0
fi
if [ "$#" -ne 1 ] || [[ -z "$1" || "$1" == -* ]]; then
    echo "Usage: bash scripts/dev/run-switch.sh <switch-ip>" >&2
    exit 2
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="$(tr -d '[:space:]' < "$REPO_DIR/docker/switch-image.txt")"
if [[ ! "$IMAGE" =~ ^devkitpro/devkita64@sha256:[0-9a-f]{64}$ ]]; then
    echo "Invalid image pin in docker/switch-image.txt" >&2
    exit 1
fi
if [ ! -s "$REPO_DIR/out/switch/switch/lba2cc/lba2cc.nro" ]; then
    echo "Switch NRO not found. Run bash scripts/dev/build-switch.sh first." >&2
    exit 1
fi
if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    echo "Docker is required and its daemon must be running." >&2
    exit 1
fi

echo "Activate NetLoader (Y) in the Homebrew Menu on $1."
echo "Uploading to sdmc:/switch/lba2cc/lba2cc.nro; Ctrl-C stops log streaming."
# An explicit address avoids LAN broadcasts; publish the reverse log connection.
exec docker run --rm --init --interactive \
    --user "$(id -u):$(id -g)" \
    --publish 28771:28771/tcp \
    --volume "$REPO_DIR/out/switch:/app:ro" --workdir /app \
    "$IMAGE" nxlink --address "$1" --path lba2cc/lba2cc.nro \
    --server switch/lba2cc/lba2cc.nro
