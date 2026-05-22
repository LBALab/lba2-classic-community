#!/usr/bin/env bash
# Robustness: a bad --load path must fail gracefully (clean exit + message), not hang
# or crash. Needs no save fixture.
TESTNAME=negative
. "$(dirname "$0")/lib.sh"
precheck

out="$(mktemp)"
trap 'rm -f "$out"' EXIT

ctl --load /no/such/save.lba --exit >"$out" 2>&1
rc=$?
[ "$rc" -ne 124 ] || fail "hung on a bad --load path"
[ "$rc" -ne 139 ] || fail "crashed (segfault) on a bad --load path"
grep -q "save not found" "$out" || fail "no 'save not found' diagnostic on stdout/stderr"

pass "bad --load handled cleanly (exit $rc, diagnostic printed)"
