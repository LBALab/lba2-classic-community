#!/usr/bin/env bash
# --polyrec: trigger the existing polygon draw-call recorder at a scripted --load/--tick
# state (instead of the manual Alt+F9). Only meaningful in an ENABLE_POLY_RECORDING build;
# skips cleanly when the binary lacks support (e.g. the default build).
#   LBA2_BIN=build-polyrec/SOURCES/lba2cc tests/automation/run.sh   # to exercise it
TESTNAME=polyrec
. "$(dirname "$0")/lib.sh"
precheck
need_save

rec="$(mktemp --suffix=.lba2polyrec)"; out="$(mktemp)"
rm -f "$rec" # the harness writes it; start absent
trap 'rm -f "$rec" "$out"' EXIT

ctl --load "$LBA2_TEST_SAVE" --tick 3 --polyrec "$rec" --exit >"$out" 2>&1
rc=$?
[ "$rc" -ne 124 ] || fail "hung"
grep -q "polyrec ignored: built without" "$out" && skip "binary built without ENABLE_POLY_RECORDING"

[ -s "$rec" ] || fail "no recording produced"
magic=$(head -c8 "$rec")
[ "$magic" = "LBA2PREC" ] || fail "bad recording magic: '$magic'"

pass "recording written ($(wc -c <"$rec") bytes)"
