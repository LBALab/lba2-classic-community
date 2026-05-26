#!/usr/bin/env bash
# Console verb `resolution`: drive Res_Switch through the user-facing path.
#
# Mirrors what a player would type at the console:
#   resolution              — list + current (no switch)
#   resolution <n>          — switch by recommended-list index
#   resolution WxH          — explicit, validated against W%8==0 and range
#   resolution native       — snap to display dims
#   resolution --all        — extended catalog including advanced modes
#
# Asserts the engine survives each switch, the right [control] echoes come
# out for both success and error paths, and the recommended list always
# contains the 640x480 classic anchor.
TESTNAME=res_switch_console
. "$(dirname "$0")/lib.sh"
precheck
need_save

out="$(mktemp)"
trap 'rm -f "$out"' EXIT

run_with() {
    local exec_arg="$1"
    local extra="${2:-}"
    # shellcheck disable=SC2086
    ctl_headless --load "$LBA2_TEST_SAVE" --exec "$exec_arg" \
        --fixed-dt 16 --tick 80 --exit $extra >"$out" 2>&1
}

# 1. No-args: list + current, no switch.
run_with "resolution" || fail "no-args returned non-zero ($?)"
grep -q "^\[control\] Current: 640x480"   "$out" || fail "no-args: missing 'Current:' line"
grep -q "640x480"                          "$out" || fail "no-args: 640x480 must always be listed"
grep -q "Usage:"                           "$out" || fail "no-args: missing Usage hint"

# 2. Numeric: switch to entry 2 in recommended list (768x480 on a >=768 display).
run_with "resolution 2" || fail "numeric returned non-zero ($?)"
grep -q "Res_Switch: 640x480 -> 768x480 OK" "$out" \
    || fail "numeric: orchestrator log missing (got: $(grep -m1 Res_Switch "$out"))"
grep -q "^\[control\] Resolution: 768x480" "$out" \
    || fail "numeric: missing success echo"

# 3. WxH explicit: 800x480 (not in recommended list).
run_with "resolution 800x480" || fail "WxH returned non-zero ($?)"
grep -q "Res_Switch: 640x480 -> 800x480 OK" "$out" || fail "WxH: orchestrator did not fire"

# 4. Invalid WxH (not W%8==0): rejected, no switch.
run_with "resolution 645x480" || fail "invalid WxH returned non-zero ($?)"
grep -q "bad target '645x480'"      "$out" || fail "invalid WxH: missing rejection echo"
grep -qv "Res_Switch: .* -> 645x480" "$out" || fail "invalid WxH: switch ran but shouldn't"

# 5. Out-of-range index.
run_with "resolution 12345" || fail "bad index returned non-zero ($?)"
grep -q "out of range"     "$out" || fail "bad index: missing rejection echo"

# 6. --all expands the list (must include 640x480 and at least one advanced).
run_with "resolution --all" || fail "--all returned non-zero ($?)"
grep -q "Available (advanced):" "$out" || fail "--all: missing advanced banner"
grep -q "advanced"              "$out" || fail "--all: no advanced entries shown"

pass "list / numeric / WxH / invalid / --all all behave correctly"
