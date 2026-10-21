#!/usr/bin/env bash
# projection corpus: capture the per-save projection-pipeline FNV-1a digest
# for every committed save in tests/savegame/corpus/, byte-compare to the
# committed golden. Phase 1.E of the widescreen plan — comprehensive
# regression net across 50 diverse game states.
#
# Width selection: LBA2_PROJREC_WIDTH (default 640) picks which committed
# golden to compare against. Set to 768 against a wide build to validate the
# Phase 3 wide path; the binary's compiled RESOLUTION_X must match.
#
# Any drift on any save fails the test and points at the regressed hash by
# save name.
#
# Two runs. The first is on the defaults. The second switches the terrain-seams
# retail fix off (fix_terrain_seams 0), which draws the outdoor ground on the
# float path again, and is held to its own golden: the one main had before the
# fix, kept byte for byte, so it shows the switch gives back the original.
#
# Local-only (needs retail data + game directory). ~6-10 seconds total
# (50 saves × 5 ticks each). Regenerate with:
#   LBA2_PROJECTION_REGEN=1 bash tests/automation/test_projection_corpus.sh
# or:
#   bash scripts/dev/regen_projrec_baselines.sh

TESTNAME=projection_corpus
. "$(dirname "$0")/lib.sh"
precheck

CORPUS="$REPO/tests/savegame/corpus/saves/steam_classic_2023"
: "${LBA2_PROJREC_WIDTH:=640}"
GOLDEN="$REPO/tests/projection/baselines/corpus_${LBA2_PROJREC_WIDTH}x480.projrec.hash"
[ -d "$CORPUS" ] || skip "corpus dir not present: $CORPUS"
[ -f "$GOLDEN" ] || [ "${LBA2_PROJECTION_REGEN:-}" = "1" ] \
    || skip "no committed golden for width ${LBA2_PROJREC_WIDTH}: $(basename "$GOLDEN")"

# corpus_run <out> [exec]: one line per save, as the golden has them.
corpus_run() {
    local out="$1" ex="${2:-}"
    {
        echo "# projrec-corpus-hash v1"
        echo "# 5-tick deterministic replay (--fixed-dt 16 --tick 5) per committed save,"
        echo "# at render width ${LBA2_PROJREC_WIDTH} (the binary's compiled RESOLUTION_X)."
        echo "# Regenerate with: LBA2_PROJECTION_REGEN=1 bash tests/automation/test_projection_corpus.sh"
    } > "$out"

    n=0
    fail_saves=""
    for save in "$CORPUS"/*.LBA "$CORPUS"/*.lba; do
        [ -f "$save" ] || continue
        name="$(basename "$save")"
        tmp="$(mktemp -t "${TESTNAME}.${name}.XXXXXX.hash")"
        set -- --game-dir "$LBA2_GAME_DIR" --load "$save" --fixed-dt 16 --tick 5 \
               --projection-hash "$tmp" --exit
        [ -n "$ex" ] && set -- "$@" --exec "$ex"
        if ctl_headless "$@" >/dev/null 2>&1; then
            line="$(grep '^SCENE' "$tmp" 2>/dev/null || true)"
            if [ -n "$line" ]; then
                events="$(echo "$line" | awk '{print $2}' | sed 's/events=//')"
                fnv1a="$(echo "$line" | awk '{print $3}' | sed 's/fnv1a=//')"
                printf "%-32s events=%-9s fnv1a=%s\n" "$name" "$events" "$fnv1a" >> "$out"
                n=$((n + 1))
            else
                fail_saves="$fail_saves $name(no-hash)"
            fi
        else
            fail_saves="$fail_saves $name(harness-rc)"
        fi
        rm -f "$tmp"
    done

    if [ -n "$fail_saves" ]; then
        rm -f "$out"
        fail "saves failed to capture:$fail_saves"
    fi
    [ "$n" -gt 0 ] || { rm -f "$out"; fail "no saves processed"; }
}

# The second golden exists at 640 only; the wide path has no original to keep.
GOLDEN_OFF="$REPO/tests/projection/baselines/corpus_${LBA2_PROJREC_WIDTH}x480.seams-off.projrec.hash"
OFF_ARM=0
if [ -f "$GOLDEN_OFF" ] || [ "${LBA2_PROJECTION_REGEN:-}" = "1" ]; then
    OFF_ARM=1
fi

OUT="$(mktemp -t "${TESTNAME}.XXXXXX.hash")"
corpus_run "$OUT"
if [ "$OFF_ARM" = 1 ]; then
    OUT_OFF="$(mktemp -t "${TESTNAME}.off.XXXXXX.hash")"
    corpus_run "$OUT_OFF" "fix_terrain_seams 0"
else
    OUT_OFF=""
fi

# projrec_compare passes or fails the whole test, so it can only judge the last
# run; both are captured first and it judges a single file listing both.
if [ -n "$OUT_OFF" ]; then
    if [ "${LBA2_PROJECTION_REGEN:-}" = "1" ]; then
        cp "$OUT_OFF" "$GOLDEN_OFF"
        echo "regenerated golden: $(basename "$GOLDEN_OFF")"
    elif ! diff <(grep -v '^#' "$OUT_OFF") <(grep -v '^#' "$GOLDEN_OFF") >/dev/null; then
        rm -f "$OUT"
        fail "with fix_terrain_seams 0, projection hash diverges from the original's golden — see $OUT_OFF vs $GOLDEN_OFF"
    fi
    rm -f "$OUT_OFF"
fi
projrec_compare "$OUT" "$GOLDEN"
