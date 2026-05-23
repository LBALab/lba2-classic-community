#!/usr/bin/env bash
# Generate projrec baseline fixtures for the committed save corpus and the
# attract-mode demo. Local-only — needs the built binary and retail game data.
#
# Outputs (filenames tagged by render width):
#   tests/projection/baselines/corpus_${WIDTH}x480.projrec.hash   (50 saves, ~3 KB)
#   tests/projection/baselines/demo_${WIDTH}x480.projrec.hash     (full demo, ~70 B)
#
# Env:
#   LBA2_BIN           path to the built binary (required)
#   LBA2_GAME_DIR      retail data dir (required)
#   LBA2_PROJREC_WIDTH render width tag for output filenames (default 640).
#                      Must match the binary's compiled RESOLUTION_X; the script
#                      does not detect it, the caller knows which binary they
#                      built. Phase 3 of the widescreen plan uses 768 against
#                      a wide build (cmake ... -DRESOLUTION_X=768).
#   PROJREC_SKIP_DEMO  if set, skip the demo run (which is ~35 seconds)
#
# Usage:
#   # Narrow (default 640x480):
#   LBA2_BIN=build/SOURCES/lba2cc LBA2_GAME_DIR=/path/to/data \
#     bash scripts/dev/regen_projrec_baselines.sh
#
#   # Wide:
#   LBA2_BIN=build-wide/SOURCES/lba2cc LBA2_GAME_DIR=/path/to/data \
#     LBA2_PROJREC_WIDTH=768 \
#     bash scripts/dev/regen_projrec_baselines.sh

set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$DIR/../.." && pwd)"
CORPUS="$REPO/tests/savegame/corpus/saves/steam_classic_2023"
BASELINES="$REPO/tests/projection/baselines"

: "${LBA2_BIN:?set LBA2_BIN to the built lba2cc binary}"
: "${LBA2_GAME_DIR:?set LBA2_GAME_DIR to the retail data dir}"
: "${LBA2_PROJREC_WIDTH:=640}"

[ -x "$LBA2_BIN" ] || { echo "no binary: $LBA2_BIN" >&2; exit 1; }
[ -d "$LBA2_GAME_DIR" ] || { echo "no game dir: $LBA2_GAME_DIR" >&2; exit 1; }
[ -d "$CORPUS" ] || { echo "no corpus dir: $CORPUS" >&2; exit 1; }

WIDTH_TAG="${LBA2_PROJREC_WIDTH}x480"

mkdir -p "$BASELINES"

# Per-save hash via 5-tick deterministic replay. The capture file is
# rewritten by each run; we extract the one SCENE line and append a
# formatted entry to the corpus output.
run_one() {
    local save="$1" tmp="$2"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$LBA2_BIN" \
        --game-dir "$LBA2_GAME_DIR" \
        --load "$save" \
        --fixed-dt 16 --tick 5 \
        --projection-hash "$tmp" --exit \
        >/dev/null 2>&1
}

corpus_out="$BASELINES/corpus_${WIDTH_TAG}.projrec.hash"
echo "# projrec-corpus-hash v1" > "$corpus_out"
echo "# 5-tick deterministic replay (--fixed-dt 16 --tick 5) per committed save," >> "$corpus_out"
echo "# at render width ${LBA2_PROJREC_WIDTH} (the binary's compiled RESOLUTION_X)." >> "$corpus_out"
echo "# Regenerate with: scripts/dev/regen_projrec_baselines.sh" >> "$corpus_out"

declare -a saves=()
while IFS= read -r f; do saves+=("$f"); done \
    < <(find "$CORPUS" \( -name "*.LBA" -o -name "*.lba" \) | sort)

echo "[regen] $(echo ${#saves[@]}) saves in corpus" >&2
n=0
for save in "${saves[@]}"; do
    name="$(basename "$save")"
    tmp="$(mktemp -t projrec.XXXXXX.hash)"
    if run_one "$save" "$tmp"; then
        # Parse "SCENE events=N fnv1a=X" from the hash output.
        line="$(grep '^SCENE' "$tmp" || true)"
        if [ -n "$line" ]; then
            events="$(echo "$line" | awk '{print $2}' | sed 's/events=//')"
            fnv1a="$(echo "$line" | awk '{print $3}' | sed 's/fnv1a=//')"
            printf "%-32s events=%-9s fnv1a=%s\n" "$name" "$events" "$fnv1a" >> "$corpus_out"
            n=$((n + 1))
        else
            echo "[regen] WARN $name: no SCENE line in hash" >&2
        fi
    else
        echo "[regen] WARN $name: harness exited non-zero" >&2
    fi
    rm -f "$tmp"
done
echo "[regen] corpus: $n / ${#saves[@]} saves -> $corpus_out" >&2

# Attract-mode snapshot: a 30000-tick replay of --demo. Slower than the
# corpus (~35 seconds) but covers cross-scene transitions and a much larger
# iso-projection sample than any single save reaches.
if [ -z "${PROJREC_SKIP_DEMO:-}" ]; then
    echo "[regen] demo: running 30000-tick attract-reel snapshot (~35 seconds)..." >&2
    demo_out="$BASELINES/demo_${WIDTH_TAG}.projrec.hash"
    tmp="$(mktemp -t projrec.XXXXXX.hash)"
    # Attract mode loops in --demo (no natural LM_THE_END exit), so we cap
    # at 30000 ticks (~8 min game time at 16ms/tick). That captures 135
    # scene/inventory transitions worth of SETISO variations plus millions
    # of iso projections — meaningful coverage without paying for 2+ minutes
    # of replay each regen. Exterior 3D coverage comes from the per-save
    # corpus, not this demo (attract mode is iso-only).
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$LBA2_BIN" \
        --game-dir "$LBA2_GAME_DIR" \
        --demo --fixed-dt 16 --tick 30000 \
        --projection-hash "$tmp" --exit \
        >/dev/null 2>&1
    {
        echo "# projrec-hash v1"
        echo "# 30000-tick attract-reel snapshot (--demo --fixed-dt 16 --tick 30000)"
        echo "# at render width ${LBA2_PROJREC_WIDTH} (the binary's compiled RESOLUTION_X)."
        echo "# Regenerate with: scripts/dev/regen_projrec_baselines.sh"
        grep '^SCENE' "$tmp"
    } > "$demo_out"
    rm -f "$tmp"
    echo "[regen] demo -> $demo_out" >&2
else
    echo "[regen] PROJREC_SKIP_DEMO set, skipping demo" >&2
fi

echo "[regen] done" >&2
