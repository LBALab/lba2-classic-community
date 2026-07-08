#!/usr/bin/env bash
# `input` console command: the harness can drive the hero's input without a keyboard.
#
# Injecting held movement (`input up 60`) must actually walk the hero, where an idle run does not.
# Proves input injection reaches the sim (not just sets a flag). Two runs from the same tracked save:
# a baseline (no input) and a walk (input up 60), compared by how far the hero moved.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=input_injection
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

start="$(mktemp)"; base="$(mktemp)"; walk="$(mktemp)"
trap 'rm -f "$start" "$base" "$walk"' EXIT

ctl_headless --load "$FIX" --fixed-dt 16 --tick 2  --dump-state "$start" --exit >/dev/null 2>&1 || fail "start run: exit $?"
ctl_headless --load "$FIX" --fixed-dt 16 --tick 62 --dump-state "$base" --exit >/dev/null 2>&1 || fail "baseline run: exit $?"
ctl_headless --load "$FIX" --fixed-dt 16 --exec "input up 60" --tick 62 --dump-state "$walk" --exit >/dev/null 2>&1 \
    || fail "walk run: exit $?"

read -r db dw < <(python3 - "$start" "$base" "$walk" <<'PY'
import json, math, sys
h = lambda f: json.load(open(f))["hero"]
s, b, w = h(sys.argv[1]), h(sys.argv[2]), h(sys.argv[3])
print(f'{math.hypot(b["x"]-s["x"], b["z"]-s["z"]):.0f} {math.hypot(w["x"]-s["x"], w["z"]-s["z"]):.0f}')
PY
)

# The walk must move the hero substantially, and clearly more than the idle baseline.
[ "$dw" -gt 200 ] && [ "$dw" -gt $((db + 150)) ] \
    || fail "input up did not walk the hero (baseline moved $db, walk moved $dw)"

pass "input injection walks the hero (baseline $db, walk $dw units)"
