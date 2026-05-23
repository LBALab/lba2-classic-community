#!/usr/bin/env bash
# projection demo: capture the projection-pipeline FNV-1a digest of a
# 30000-tick attract-mode snapshot, byte-compare to the committed golden.
#
# Attract mode loops indefinitely (no natural exit), so we fix the budget
# at 30000 ticks (~35 seconds wall-clock). That's 135 scene/inventory
# transitions worth of SETISO variations plus millions of iso projections
# — bonus coverage on top of the per-save corpus test. Exterior 3D
# coverage comes from the corpus (attract mode is iso-only).
#
# Opt-in via PROJREC_RUN_DEMO=1 — even at 35s this is the slowest test in
# the harness suite. Skip by default.
#
# Local-only (needs retail data). Regenerate with:
#   LBA2_PROJECTION_REGEN=1 PROJREC_RUN_DEMO=1 bash tests/automation/test_projection_demo.sh
# or:
#   bash scripts/dev/regen_projrec_baselines.sh

TESTNAME=projection_demo
. "$(dirname "$0")/lib.sh"
precheck

[ "${PROJREC_RUN_DEMO:-}" = "1" ] || skip "demo test is opt-in (set PROJREC_RUN_DEMO=1; ~35 seconds)"

: "${LBA2_PROJREC_WIDTH:=640}"
GOLDEN="$REPO/tests/projection/baselines/demo_${LBA2_PROJREC_WIDTH}x480.projrec.hash"
[ -f "$GOLDEN" ] || [ "${LBA2_PROJECTION_REGEN:-}" = "1" ] \
    || skip "no committed golden for width ${LBA2_PROJREC_WIDTH}: $(basename "$GOLDEN")"
OUT="$(mktemp -t "${TESTNAME}.XXXXXX.hash")"

ctl_headless --game-dir "$LBA2_GAME_DIR" \
             --demo --fixed-dt 16 --tick 30000 \
             --projection-hash "$OUT" --exit \
             >/dev/null 2>&1 \
    || { rm -f "$OUT"; fail "demo harness returned non-zero ($?)"; }

# Strip any non-SCENE lines the harness may have emitted, then format the
# fixture body (the committed file has the same header).
TMP="$(mktemp -t "${TESTNAME}.body.XXXXXX.hash")"
{
    echo "# projrec-hash v1"
    echo "# 30000-tick attract-reel snapshot (--demo --fixed-dt 16 --tick 30000)"
    echo "# at render width ${LBA2_PROJREC_WIDTH} (the binary's compiled RESOLUTION_X)."
    echo "# Regenerate with: scripts/dev/regen_projrec_baselines.sh"
    grep '^SCENE' "$OUT" 2>/dev/null
} > "$TMP"
mv "$TMP" "$OUT"

projrec_compare "$OUT" "$GOLDEN"
