#!/usr/bin/env bash
# --exec: console commands mutate observable state. Uses a ';' batch to also cover
# command splitting. NOTE: only non-modal commands work headless — commands that open
# a cinematic/video/dialogue (give <item>, playvideo, credits, slide) block a --exit run
# because nothing dismisses them. `cube` and `give clover` are non-modal.
TESTNAME=exec
. "$(dirname "$0")/lib.sh"
precheck
need_save

json="$(mktemp)"
trap 'rm -f "$json"' EXIT

ctl --load "$LBA2_TEST_SAVE" --exec "cube 100; give clover 3" --tick 3 --dump-state "$json" --exit \
    >/dev/null 2>&1 || fail "non-zero exit ($?) — hang or crash"

cube=$(jget "$json" "d['scene']['cube']")
clover=$(jget "$json" "d['inventory']['clover_boxes']")

[ "$cube" = "100" ] || fail "cube=$cube (want 100; exec 'cube 100' did not take effect)"
[ "$clover" = "3" ] || fail "clover_boxes=$clover (want 3; exec 'give clover 3' did not take effect)"

pass "cube=$cube clover_boxes=$clover (batch exec applied)"
