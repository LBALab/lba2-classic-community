#!/usr/bin/env bash
# High-frame-rate movement invariance (issue #358).
#
# Locomotion is baked into animation keyframe translations delivered once per SIMULATED frame
# (ObjectSetInterDep). Above ~60 fps the animation clock would otherwise advance in sub-step
# slivers and movement speeds up or freezes. The fixed-timestep simulation (SOURCES/PERSO.CPP
# -> Timer_PlanSimSteps) runs `elapsed / FixedTimestep` sub-steps per frame: at high frame
# rates that is 0 (skip the sim, re-present, carry the time), so the distance a scripted actor
# walks over a fixed span of game-time does not depend on how fast we render above the cap.
#
# --fixed-dt N pins the per-tick game-clock step to N ms (~1000/N fps, deterministic). We walk
# the SAME 3200 ms of game-time at two rates and compare how far the scene's movers travel:
#     ~120 fps: --fixed-dt 8  --tick 400   the planner runs 0 sub-steps on every other frame,
#                                          so this is the ONLY test that exercises the skip/carry
#                                          branch (Timer_PlanSimSteps returning 0 with a carry)
#     ~60 fps:  --fixed-dt 16 --tick 200   the planner runs exactly one sub-step per frame
# Both collapse to a 16 ms effective sim step, so the movers travel the same distance. Disable
# the throttle and the ~120 fps run reverts to the per-frame path and over-walks -- that
# regression is what this gates. The low-fps counterpart (dt16 vs dt64) is test_move_substep.sh.
#
# Assumes the default throttle (FixedTimestep=16); the run does not touch it, so it neither
# depends on nor rewrites lba2.cfg. Local-only (needs retail data + a save); skips cleanly
# otherwise. Not in CI.
TESTNAME=move_framerate
. "$(dirname "$0")/lib.sh"
precheck
need_save

base="$(mktemp)"; a="$(mktemp)"; c="$(mktemp)"
trap 'rm -f "$base" "$a" "$c"' EXIT

# Same total game-time (3200 ms) at ~60 fps and ~120 fps. Each run reloads the same save.
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 0   --dump-state "$base" --exit >/dev/null 2>&1 \
    || fail "baseline run: non-zero exit ($?)"
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 200 --dump-state "$a"    --exit >/dev/null 2>&1 \
    || fail "~60fps run: non-zero exit ($?)"
ctl --no-audio --load "$LBA2_TEST_SAVE" --fixed-dt 8  --tick 400 --dump-state "$c"    --exit >/dev/null 2>&1 \
    || fail "~120fps run: non-zero exit ($?)"

read -r d60 d120 span < <(python3 - "$base" "$a" "$c" <<'PY'
import json, math, sys
base = json.load(open(sys.argv[1]))["actors"]
def travelled(path):
    end = json.load(open(path))["actors"]
    return sum(math.hypot(e["x"] - b0["x"], e["z"] - b0["z"]) for b0, e in zip(base, end))
d60  = travelled(sys.argv[2])
d120 = travelled(sys.argv[3])
span = json.load(open(sys.argv[2]))["timer_ref_hr"] - json.load(open(sys.argv[1]))["timer_ref_hr"]
print(f"{d60:.0f} {d120:.0f} {span}")
PY
)

echo "  scripted actors travelled over ${span}ms of game-time: ~60fps=${d60}  ~120fps=${d120}"

# Measuring invariance needs actual movement; a static scene walks 0 at every rate and would
# pass trivially, hiding a regression -- skip instead of green-washing.
python3 - "$d60" <<'PY' || skip "loaded scene has no scripted movement (d60=$d60); use a save with movers"
import sys
sys.exit(0 if float(sys.argv[1]) >= 100.0 else 1)
PY

# GREEN gate: above the cap the sim step is pinned, so ~120 fps must track ~60 fps. Tolerance
# 10%: the residual is a few % (per-frame sub-effects the sim throttle does not gate); a
# throttle regression reverts ~120 fps to the per-frame path (~14% over-walk) and trips this.
python3 - "$d60" "$d120" <<'PY' || fail "high-fps movement not invariant: ~60fps=$d60 vs ~120fps=$d120 (issue #358)"
import sys
d60, d120 = float(sys.argv[1]), float(sys.argv[2])
ref = max(d60, 1.0)
sys.exit(0 if abs(d60 - d120) <= 0.10 * ref else 1)
PY

pass "high-fps movement invariant (~60fps=$d60, ~120fps=$d120)"
