#!/usr/bin/env bash
# Movement is frame-rate dependent (issue #358): end-to-end repro.
#
# Character/NPC locomotion is baked into animation keyframe translations delivered
# once per rendered frame (ObjectSetInterDep), so the distance a scripted actor walks
# over a fixed span of *game-time* must not depend on the frame rate. --fixed-dt N
# pins the per-tick game-clock step to N ms, i.e. emulates ~1000/N fps deterministically.
# We run the SAME save for the SAME game-time (3200 ms) at dt=16 (~60 fps) and dt=64
# (~16 fps) and compare how far the scene's scripted movers travel.
#
# Today the two totals differ well beyond tolerance, which is the bug (RED). See
# docs/MOVEMENT_FRAMERATE.md for the mechanism and the fixed-timestep fix.
#
# NOTE: --fixed-dt drives the *simulation* step directly, so this exercises the
# unit-level coupling end-to-end. Once the fixed-timestep fix lands, the simulation
# is pinned to a constant step regardless of render rate; a follow-up will re-point
# this test at the render-rate knob so it becomes the GREEN gate. For now it is the
# executable, real-data proof of the coupling.
#
# Local-only (needs retail data + a save); skips cleanly otherwise. Not in CI.
TESTNAME=move_framerate
. "$(dirname "$0")/lib.sh"
precheck
need_save

base="$(mktemp)"; a="$(mktemp)"; b="$(mktemp)"
trap 'rm -f "$base" "$a" "$b"' EXIT

# Same total game-time (3200 ms) at two frame rates.
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 0   --dump-state "$base" --exit >/dev/null 2>&1 \
    || fail "baseline run: non-zero exit ($?)"
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 200 --dump-state "$a"    --exit >/dev/null 2>&1 \
    || fail "60fps run: non-zero exit ($?)"
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 64 --tick 50  --dump-state "$b"    --exit >/dev/null 2>&1 \
    || fail "16fps run: non-zero exit ($?)"

# Total distance all actors travelled from the baseline, at each frame rate.
read -r d60 d16 span < <(python3 - "$base" "$a" "$b" <<'PY'
import json, math, sys
base = json.load(open(sys.argv[1]))["actors"]
def travelled(path):
    end = json.load(open(path))["actors"]
    return sum(math.hypot(e["x"] - b0["x"], e["z"] - b0["z"]) for b0, e in zip(base, end))
d60 = travelled(sys.argv[2])
d16 = travelled(sys.argv[3])
# Both runs cover the same game-time; report it so a failure is self-explanatory.
span = json.load(open(sys.argv[3]))["timer_ref_hr"] - json.load(open(sys.argv[1]))["timer_ref_hr"]
print(f"{d60:.0f} {d16:.0f} {span}")
PY
)

echo "  scripted actors travelled over ${span}ms of game-time: ~60fps=${d60}  ~16fps=${d16}"

# Same game-time must walk the same distance. Fails today (that is #358).
python3 - "$d60" "$d16" <<'PY' || fail "frame-rate dependent movement: 60fps=$d60 vs 16fps=$d16 (issue #358)"
import sys
d60, d16 = float(sys.argv[1]), float(sys.argv[2])
ref = max(d60, 1.0)
sys.exit(0 if abs(d60 - d16) <= 0.05 * ref else 1)
PY

pass "movement frame-rate invariant (60fps=$d60, 16fps=$d16)"
