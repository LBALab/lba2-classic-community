#!/usr/bin/env bash
# --demo: deterministic scripted-playthrough fixture. Cube 193 is the retail attract scene
# (GAMEMENU.CPP:2435 loads NewCube=193 with DemoSlide for the menu's idle demo) — an authored
# "demo scene" (SCENES.md "Demo scenes (193-221)", a Twinsen's-House copy with demo scripts).
# Under --demo it runs itself from its Life/Track scripts with no player input: the hero walks an
# authored path while the scene plays. With --fixed-dt that run is deterministic, making it a
# regression guardrail far broader than an 8-tick settled save — it exercises the Life/Track
# interpreters, animation, and movement over time.
#
# Two assertions:
#   (1) Two $TICKS runs are byte-identical on simulation state (and including timer_ref_hr
#       since Timer_EnableFixedDt() resets it -- PR #175 -- so the fresh-start clock is pinned
#       to 0 just like a restored save).
#   (2) The simulation advanced between tick 1 and tick $TICKS (scripts ran, not frozen).
# Plus a separate fixture for the natural-end path:
#   (3) Cube 218 ("The Dark Monk statue (last)") with a generous tick budget reaches
#       LM_THE_END before the budget, MainLoop returns to main(), and Control_End() (PR #178)
#       still produces a --dump-state with scene.cube == 218 and tick < budget. This covers
#       the Control_End wiring that the (1)/(2) test by design avoids (cube 193 with 500 ticks
#       stays clear of any natural exit).
TESTNAME=demo
. "$(dirname "$0")/lib.sh"
precheck

DEMO_CUBE=193
TICKS=500
END_CUBE=218
END_BUDGET=3000

a="$(mktemp)"; b="$(mktemp)"; c="$(mktemp)"; e="$(mktemp)"
trap 'rm -f "$a" "$b" "$c" "$e"' EXIT

ctl --exec "cube $DEMO_CUBE" --demo --tick 1     --fixed-dt 16 --dump-state "$a" --exit >/dev/null 2>&1 \
    || fail "demo tick 1 run: non-zero exit ($?)"
ctl --exec "cube $DEMO_CUBE" --demo --tick $TICKS --fixed-dt 16 --dump-state "$b" --exit >/dev/null 2>&1 \
    || fail "demo tick $TICKS run: non-zero exit ($?)"
ctl --exec "cube $DEMO_CUBE" --demo --tick $TICKS --fixed-dt 16 --dump-state "$c" --exit >/dev/null 2>&1 \
    || fail "demo tick $TICKS rerun: non-zero exit ($?)"

# Determinism: the two $TICKS runs must agree on all simulation state including timer_ref_hr.
# Drop only fps (wall-clock-derived) and the console log (run-specific).
python3 - "$b" "$c" <<'PY' || fail "scripted playthrough not reproducible (runs differ)"
import json, sys
def norm(p):
    d = json.load(open(p))
    for k in ("fps", "log"):
        d.pop(k, None)
    return d
sys.exit(0 if norm(sys.argv[1]) == norm(sys.argv[2]) else 1)
PY

# Liveness: the simulation advanced between tick 1 and tick $TICKS (scripts ran, not frozen).
python3 - "$a" "$b" <<'PY' || fail "scene did not advance (tick 1 == tick N): demo idle or hung"
import json, sys
def norm(p):
    d = json.load(open(p))
    for k in ("fps", "log", "tick"):
        d.pop(k, None)
    return d
sys.exit(0 if norm(sys.argv[1]) != norm(sys.argv[2]) else 1)
PY

hl=$(jget "$a" "d['hero']['last_frame']"); hl2=$(jget "$b" "d['hero']['last_frame']")

# Natural-end: cube 218's Life script fires LM_THE_END (// credits) within ~1100 ticks, so
# with --tick $END_BUDGET MainLoop returns naturally, well before the budget. Control_End()
# should capture the final state. The dump must show (a) tick < budget (natural exit, not
# budget hit) and (b) scene.cube == 218.
ctl --exec "cube $END_CUBE" --demo --tick $END_BUDGET --fixed-dt 16 --dump-state "$e" --exit >/dev/null 2>&1 \
    || fail "natural-end run for cube $END_CUBE: non-zero exit ($?)"

python3 - "$e" "$END_BUDGET" "$END_CUBE" <<'PY' || fail "Control_End did not capture LM_THE_END natural exit"
import json, sys
d = json.load(open(sys.argv[1]))
budget = int(sys.argv[2]); want_cube = int(sys.argv[3])
if d["tick"] >= budget: sys.exit(2)            # budget was hit, no natural exit
if d["scene"]["cube"] != want_cube: sys.exit(3) # ended in wrong cube
sys.exit(0)
PY

et=$(jget "$e" "d['tick']")
pass "cube $DEMO_CUBE $TICKS ticks reproducible+advancing (anim $hl->$hl2); cube $END_CUBE LM_THE_END at t=$et captured via Control_End"
