#!/usr/bin/env bash
# --load: restore a save before the loop, then advance and dump.
# Asserts the loaded scene is populated and the tick count is exact.
TESTNAME=load
. "$(dirname "$0")/lib.sh"
precheck
need_save

json="$(mktemp)"
trap 'rm -f "$json"' EXIT

ctl --load "$LBA2_TEST_SAVE" --tick 2 --dump-state "$json" --exit >/dev/null 2>&1 \
    || fail "non-zero exit ($?) — hang or crash"

tick=$(jget "$json" "d['tick']")
cube=$(jget "$json" "d['scene']['cube']")
island=$(jget "$json" "d['scene']['island']")
nobj=$(jget "$json" "d['scene']['num_objects']")
life=$(jget "$json" "d['hero']['life']")

[ "$tick" = "2" ] || fail "tick=$tick (want 2)"
[ "$nobj" -gt 0 ] || fail "loaded scene has no objects (num_objects=$nobj)"
[ "$cube" -ge 0 ] || fail "invalid cube=$cube"

pass "island=$island cube=$cube objs=$nobj hero_life=$life"
