#!/usr/bin/env bash
# --demo: deterministic scripted-playthrough fixture. Cube 193 is the retail attract scene
# (GAMEMENU.CPP:2435 loads NewCube=193 with DemoSlide for the menu's idle demo) — an authored
# "demo scene" (SCENES.md "Demo scenes (193-221)", a Twinsen's-House copy with demo scripts).
# Under --demo it runs itself from its Life/Track scripts with no player input: the hero walks an
# authored path while the scene plays. With --fixed-dt that run is deterministic, making it a
# regression guardrail far broader than an 8-tick settled save — it exercises the Life/Track
# interpreters, animation, and movement over time.
#
# 500 ticks (~8 s of game time) stays clear of the cutscene the demo ends on (which would block a
# --exit run; the demo scenes are short clips). Asserts (1) the run is reproducible — two runs
# byte-identical on simulation state — and (2) it is live — state at tick N differs from tick 1
# (scripts ran, did not idle or hang). timer_ref_hr is dropped: a fresh-start cube jump (no
# --load) does not pin the absolute clock the way a restored save does, but every simulation
# field is deterministic (verified serial and 5-way parallel).
TESTNAME=demo
. "$(dirname "$0")/lib.sh"
precheck

DEMO_CUBE=193
TICKS=500

a="$(mktemp)"; b="$(mktemp)"; c="$(mktemp)"
trap 'rm -f "$a" "$b" "$c"' EXIT

ctl --exec "cube $DEMO_CUBE" --demo --tick 1     --fixed-dt 16 --dump-state "$a" --exit >/dev/null 2>&1 \
    || fail "demo tick 1 run: non-zero exit ($?)"
ctl --exec "cube $DEMO_CUBE" --demo --tick $TICKS --fixed-dt 16 --dump-state "$b" --exit >/dev/null 2>&1 \
    || fail "demo tick $TICKS run: non-zero exit ($?)"
ctl --exec "cube $DEMO_CUBE" --demo --tick $TICKS --fixed-dt 16 --dump-state "$c" --exit >/dev/null 2>&1 \
    || fail "demo tick $TICKS rerun: non-zero exit ($?)"

# Determinism: the two $TICKS runs must agree on all simulation state. Drop wall-clock-derived
# fields (fps, timer_ref_hr) and the console log.
python3 - "$b" "$c" <<'PY' || fail "scripted playthrough not reproducible (runs differ)"
import json, sys
def norm(p):
    d = json.load(open(p))
    for k in ("fps", "timer_ref_hr", "log"):
        d.pop(k, None)
    return d
sys.exit(0 if norm(sys.argv[1]) == norm(sys.argv[2]) else 1)
PY

# Liveness: the simulation advanced between tick 1 and tick $TICKS (scripts ran, not frozen).
python3 - "$a" "$b" <<'PY' || fail "scene did not advance (tick 1 == tick N): demo idle or hung"
import json, sys
def norm(p):
    d = json.load(open(p))
    for k in ("fps", "timer_ref_hr", "log", "tick"):
        d.pop(k, None)
    return d
sys.exit(0 if norm(sys.argv[1]) != norm(sys.argv[2]) else 1)
PY

hl=$(jget "$a" "d['hero']['last_frame']"); hl2=$(jget "$b" "d['hero']['last_frame']")
pass "cube $DEMO_CUBE demo: $TICKS ticks reproducible and advancing (hero anim frame $hl->$hl2)"
