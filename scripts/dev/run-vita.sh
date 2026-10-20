#!/usr/bin/env bash
# Upload a built Vita executable/package and optionally launch via vitacompanion.
# Usage: bash scripts/dev/run-vita.sh <vita-ip> [launch|send|vpksend]
set -euo pipefail

if [ "$#" -eq 1 ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
    sed -n '2,3s/^# //p' "$0"
    exit 0
fi
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ] || [[ ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Usage: bash scripts/dev/run-vita.sh <vita-ip> [launch|send|vpksend]" >&2
    exit 2
fi
case "${2:-launch}" in
    launch|send|vpksend) ;;
    *) echo "Unknown Vita deployment mode: $2" >&2; exit 2 ;;
esac

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

exec docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "$REPO_DIR:/src:ro" --workdir /src \
    "$IMAGE" python3 scripts/dev/vita-deploy.py "$@"
