#!/usr/bin/env bash
# projection baseline: capture the projection-pipeline FNV-1a digest of a
# deterministic 5-tick replay of Downtown.LBA, byte-compare to the committed
# golden hash. Phase 1.C of the widescreen plan — the regression net that
# Phase 2's projection-origin changes must not disturb at 640×480.
#
# Fixture: $LBA2_TEST_SAVE must be set to a deterministic exterior save (the
# default is Downtown for parity with the committed hash). The replay uses
# --fixed-dt 16 --tick 5; do not change those without regenerating the hash.
#
# Regenerate the golden with: LBA2_PROJECTION_REGEN=1 bash tests/automation/test_projection_baseline.sh

TESTNAME=projection_baseline
. "$(dirname "$0")/lib.sh"
precheck
need_save

GOLDEN="$REPO/tests/projection/baselines/downtown_640x480.projrec.hash"
OUT="$(mktemp -t "${TESTNAME}.XXXXXX.hash")"

ctl_headless --load "$LBA2_TEST_SAVE" \
    --fixed-dt 16 --tick 5 --projection-hash "$OUT" --exit \
    >/dev/null 2>&1 \
    || { rm -f "$OUT"; fail "harness returned non-zero ($?)"; }

[ -f "$OUT" ] || fail "no hash file written"

if [ "${LBA2_PROJECTION_REGEN:-}" = "1" ]; then
    mkdir -p "$(dirname "$GOLDEN")"
    cp "$OUT" "$GOLDEN"
    rm -f "$OUT"
    pass "regenerated golden: $(basename "$GOLDEN")"
fi

[ -f "$GOLDEN" ] || { rm -f "$OUT"; fail "golden not committed yet: $GOLDEN"; }

if diff -q "$OUT" "$GOLDEN" >/dev/null; then
    rm -f "$OUT"
    pass "projection hash matches golden ($(awk '/fnv1a=/{print $NF}' "$GOLDEN"))"
else
    fail "projection hash diverges from golden — see $OUT vs $GOLDEN"
fi
