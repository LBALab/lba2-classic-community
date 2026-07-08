#!/usr/bin/env bash
# Low-frame-rate movement invariance (issue #358, phase 2).
#
# Phase 1 pins the sim to at most ~60 Hz, fixing high frame rates but leaving low ones
# (a >= 16 ms frame) running one oversized step per frame, which loses game-time to the
# keyframe overshoot-discard. Phase 2 sub-steps: a frame worth N*16 ms of game-time runs
# the simulation N times at 16 ms each. So under --fixed-dt, the SAME span of game-time must
# walk the SAME distance whether the clock is stepped in 16 ms ticks or 64 ms ticks.
#
# We run a scripted mover for 3200 ms of game-time at --fixed-dt 16 (200 ticks) and at
# --fixed-dt 64 (50 ticks) and compare total actor travel. Phase 1 alone: they differ
# (that is the low-fps loss). Phase 2: they match. RED until phase 2 lands.
#
# Local-only (needs retail data + a save); skips cleanly otherwise. Not in CI.
TESTNAME=move_substep
. "$(dirname "$0")/lib.sh"
precheck
need_save

base="$(mktemp)"; a="$(mktemp)"; b="$(mktemp)"
trap 'rm -f "$base" "$a" "$b"' EXIT

# Same 3200 ms of game-time, stepped fine (dt=16) vs coarse (dt=64). Fixed-timestep on.
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 0   --dump-state "$base" --exit >/dev/null 2>&1 \
    || fail "baseline run: non-zero exit ($?)"
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 16 --exec "fixedtimestep on" --tick 200 --dump-state "$a" --exit >/dev/null 2>&1 \
    || fail "dt16 run: non-zero exit ($?)"
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 64 --exec "fixedtimestep on" --tick 50  --dump-state "$b" --exit >/dev/null 2>&1 \
    || fail "dt64 run: non-zero exit ($?)"

read -r d16 d64 span < <(python3 - "$base" "$a" "$b" <<'PY'
import json, math, sys
base = json.load(open(sys.argv[1]))["actors"]
def travelled(path):
    end = json.load(open(path))["actors"]
    return sum(math.hypot(e["x"] - b0["x"], e["z"] - b0["z"]) for b0, e in zip(base, end))
d16 = travelled(sys.argv[2])
d64 = travelled(sys.argv[3])
span = json.load(open(sys.argv[3]))["timer_ref_hr"] - json.load(open(sys.argv[1]))["timer_ref_hr"]
print(f"{d16:.0f} {d64:.0f} {span}")
PY
)

echo "  scripted actors travelled over ${span}ms: dt16=${d16}  dt64=${d64}"

python3 - "$d16" "$d64" <<'PY' || fail "low-fps sub-stepping mismatch: dt16=$d16 vs dt64=$d64 (issue #358 phase 2)"
import sys
d16, d64 = float(sys.argv[1]), float(sys.argv[2])
ref = max(d16, 1.0)
sys.exit(0 if abs(d16 - d64) <= 0.05 * ref else 1)
PY

pass "low-fps movement invariant (dt16=$d16, dt64=$d64)"
