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

OUT="$(mktemp -t "${TESTNAME}.XXXXXX.hash")"
{
    echo "# projrec-corpus-hash v1"
    echo "# 5-tick deterministic replay (--fixed-dt 16 --tick 5) per committed save,"
    echo "# at render width ${LBA2_PROJREC_WIDTH} (the binary's compiled RESOLUTION_X)."
    echo "# Regenerate with: scripts/dev/regen_projrec_baselines.sh"
} > "$OUT"

n=0
fail_saves=""
for save in "$CORPUS"/*.LBA "$CORPUS"/*.lba; do
    [ -f "$save" ] || continue
    name="$(basename "$save")"
    tmp="$(mktemp -t "${TESTNAME}.${name}.XXXXXX.hash")"
    if ctl_headless --game-dir "$LBA2_GAME_DIR" --load "$save" \
                    --fixed-dt 16 --tick 5 \
                    --projection-hash "$tmp" --exit \
                    >/dev/null 2>&1; then
        line="$(grep '^SCENE' "$tmp" 2>/dev/null || true)"
        if [ -n "$line" ]; then
            events="$(echo "$line" | awk '{print $2}' | sed 's/events=//')"
            fnv1a="$(echo "$line" | awk '{print $3}' | sed 's/fnv1a=//')"
            printf "%-32s events=%-9s fnv1a=%s\n" "$name" "$events" "$fnv1a" >> "$OUT"
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
    rm -f "$OUT"
    fail "saves failed to capture:$fail_saves"
fi

[ "$n" -gt 0 ] || { rm -f "$OUT"; fail "no saves processed"; }

projrec_compare "$OUT" "$GOLDEN"
