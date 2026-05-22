#!/usr/bin/env bash
# Robustness: a bad --load path must fail gracefully (clean exit + message), not hang
# or crash. Needs no save fixture.
TESTNAME=negative
. "$(dirname "$0")/lib.sh"
precheck

out="$(mktemp)"; out2="$(mktemp)"
trap 'rm -f "$out" "$out2"' EXIT

# Bad --load path: graceful exit + diagnostic, no hang/crash.
ctl --load /no/such/save.lba --exit >"$out" 2>&1
rc=$?
[ "$rc" -ne 124 ] || fail "hung on a bad --load path"
[ "$rc" -ne 139 ] || fail "crashed (segfault) on a bad --load path"
grep -q "save not found" "$out" || fail "no 'save not found' diagnostic on stdout/stderr"

# Non-numeric --tick: warn and treat as 0, don't hang. Fresh start (no save needed).
ctl --tick notanumber --exit >"$out2" 2>&1
rc2=$?
[ "$rc2" -ne 124 ] || fail "hung on a non-numeric --tick"
grep -q "invalid count" "$out2" || fail "no warning on non-numeric --tick"

pass "bad --load and non-numeric --tick handled cleanly"
