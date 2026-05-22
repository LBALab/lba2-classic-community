#!/usr/bin/env bash
# --dump-state: the hand-written JSON is well-formed and has the expected shape.
# Guards against malformed output (the real risk of hand-rolling JSON).
TESTNAME=dump
. "$(dirname "$0")/lib.sh"
precheck
need_save

json="$(mktemp)"
trap 'rm -f "$json"' EXIT

ctl --load "$LBA2_TEST_SAVE" --tick 2 --dump-state "$json" --exit >/dev/null 2>&1 \
    || fail "non-zero exit ($?)"

python3 - "$json" <<'PY' || fail "schema/shape check failed"
import json, sys
d = json.load(open(sys.argv[1]))
assert d.get("schema") == 1, "schema != 1"
for k in ("tick", "timer_ref_hr", "fps", "scene", "hero", "inventory", "actors", "vars", "log"):
    assert k in d, "missing top-level key: " + k
for k in ("island", "cube", "cube_mode", "num_objects"):
    assert k in d["scene"], "missing scene." + k
for k in ("x", "y", "z", "beta", "life", "body", "anim", "last_frame"):
    assert k in d["hero"], "missing hero." + k
assert isinstance(d["actors"], list)
assert "nonzero" in d["vars"]
PY

pass "valid JSON, schema=1, required keys present"
