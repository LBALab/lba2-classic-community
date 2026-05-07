#!/usr/bin/env bash
# Build and run the savegame corpus harness from any working directory.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$("$SCRIPT_DIR/repo_root.sh")"
BUILD_DIR="${LBA2_BUILD_DIR:-$REPO_ROOT/build}"
CORPUS_DIR="$REPO_ROOT/tests/savegame/corpus/saves/steam_classic_2023"
PROBE_OUT="$REPO_ROOT/tests/savegame/corpus/probe.ndjson"
DEFAULT_TIMEOUT=15
TIMEOUT="$DEFAULT_TIMEOUT"
ABIS="${ABIS:-auto,32}"
GAME_DIR_ARG=""

usage() {
  cat <<EOF
Usage: $(basename "$0") [--game-dir PATH] [--timeout N] [--abis LIST]

Runs:
  1) save_probe.py -> tests/savegame/corpus/probe.ndjson
  2) build_manifest.py
  3) run_harness.py --save-load-test matrix

Game data resolution order:
  1) --game-dir PATH
  2) LBA2_GAME_DIR
  3) local candidates: ./data, ../LBA2, ../game (from repo root)
EOF
}

has_hqr_marker() {
  local dir="$1"
  # Match the engine's discovery: HQR can live directly in $dir or one level
  # down in Common/ (the retail layout). See SOURCES/RES_DISCOVERY.CPP.
  [[ -f "$dir/lba2.hqr" || -f "$dir/LBA2.HQR" \
     || -f "$dir/Common/lba2.hqr" || -f "$dir/Common/LBA2.HQR" ]]
}

abs_path() {
  local dir="$1"
  (cd "$dir" && pwd)
}

resolve_executable() {
  local base="$1"
  if [[ -x "$base" ]]; then
    printf '%s\n' "$base"
    return 0
  fi
  if [[ -x "${base}.exe" ]]; then
    printf '%s\n' "${base}.exe"
    return 0
  fi
  return 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --game-dir)
      [[ $# -ge 2 ]] || { echo "missing value for --game-dir" >&2; exit 2; }
      GAME_DIR_ARG="$2"
      shift 2
      ;;
    --timeout)
      [[ $# -ge 2 ]] || { echo "missing value for --timeout" >&2; exit 2; }
      TIMEOUT="$2"
      shift 2
      ;;
    --abis)
      [[ $# -ge 2 ]] || { echo "missing value for --abis" >&2; exit 2; }
      ABIS="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

GAME_DIR="${GAME_DIR_ARG:-${LBA2_GAME_DIR:-}}"
if [[ -n "$GAME_DIR" && ! -d "$GAME_DIR" ]]; then
  echo "game dir does not exist: $GAME_DIR" >&2
  exit 2
fi

if [[ -z "$GAME_DIR" ]]; then
  for cand in "$REPO_ROOT/data" "$REPO_ROOT/../LBA2" "$REPO_ROOT/../game"; do
    if [[ -d "$cand" ]] && has_hqr_marker "$cand"; then
      GAME_DIR="$(abs_path "$cand")"
      break
    fi
  done
fi

if [[ -z "$GAME_DIR" ]]; then
  cat >&2 <<EOF
Unable to locate LBA2 game data.
Set --game-dir PATH or export LBA2_GAME_DIR.
Auto-discovery looked in:
  $REPO_ROOT/data
  $REPO_ROOT/../LBA2
  $REPO_ROOT/../game
EOF
  exit 2
fi

if ! has_hqr_marker "$GAME_DIR"; then
  echo "game dir missing lba2.hqr/LBA2.HQR marker: $GAME_DIR" >&2
  exit 2
fi

if [[ ! -d "$CORPUS_DIR" ]]; then
  echo "corpus dir missing: $CORPUS_DIR" >&2
  exit 2
fi

echo "[savegame-corpus] repo root: $REPO_ROOT"
echo "[savegame-corpus] build dir: $BUILD_DIR"
echo "[savegame-corpus] game dir:  $GAME_DIR"
echo "[savegame-corpus] timeout:   $TIMEOUT"
echo "[savegame-corpus] abis:      $ABIS"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
cmake --build "$BUILD_DIR" --target lba2 save_decompress

SAVE_DECOMPRESS_BIN="$(resolve_executable "$BUILD_DIR/tools/save_decompress")" || {
  echo "save_decompress binary not found under $BUILD_DIR/tools" >&2
  exit 1
}
LBA2_BIN="$(resolve_executable "$BUILD_DIR/SOURCES/lba2")" || {
  echo "lba2 binary not found under $BUILD_DIR/SOURCES" >&2
  exit 1
}

LBA2_SAVE_TEST_DIR="$CORPUS_DIR" \
LBA2_SAVE_DECOMPRESS="$SAVE_DECOMPRESS_BIN" \
python3 "$REPO_ROOT/scripts/save_probe.py" --recursive --json-lines "$CORPUS_DIR" > "$PROBE_OUT"

LBA2_SAVE_TEST_DIR="$CORPUS_DIR" \
python3 "$REPO_ROOT/tests/savegame/corpus/build_manifest.py"

python3 "$REPO_ROOT/tests/savegame/corpus/run_harness.py" \
  --lba2 "$LBA2_BIN" \
  --game-dir "$GAME_DIR" \
  --timeout "$TIMEOUT" \
  --abis "$ABIS"
