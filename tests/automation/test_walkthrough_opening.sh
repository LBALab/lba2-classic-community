#!/usr/bin/env bash
# Walkthrough e2e: drive the OPENING of the game through the engine, from a cold
# boot, with no savegame. A savegame would teleport past the game; this drives
# the real thing and asserts the engine's own reaction at each beat.
#
# The four beats mirror the start of any Twinsen's Odyssey walkthrough:
#   1. New game starts you in Twinsen's house (island 0, cube 0).
#   2. You can walk.
#   3. Stepping into a scenaric trigger fires the hero's Life script and
#      advances quest state (game vars flip).
#   4. Walking into the doorway leaves the house for the next scene (cube 49).
#
# Everything is driven with stock console verbs (input / teleport) under
# --fixed-dt for determinism. Zone coordinates were read off the `zonelist`
# console command (cube 0): scenaric zone Num=1 and the cube-change zone to 49.
#
# Local-only: needs retail data (LBA2_GAME_DIR) and a build. No save required.
TESTNAME=walkthrough_opening
. "$(dirname "$0")/lib.sh"
precheck

d0="$(mktemp)"; d1="$(mktemp)"; d2="$(mktemp)"; d3="$(mktemp)"
trap 'rm -f "$d0" "$d1" "$d2" "$d3"' EXIT

# ── Beat 1: a cold boot lands in Twinsen's house, in a fresh new-game state ──
ctl_headless --fixed-dt 16 --tick 5 --dump-state "$d0" --exit >/dev/null 2>&1 \
    || fail "cold boot: non-zero exit ($?)"
isl=$(jget "$d0" "d['scene']['island']"); cube=$(jget "$d0" "d['scene']['cube']")
[ "$isl" = "0" ] && [ "$cube" = "0" ] \
    || fail "cold boot not in the house: island=$isl cube=$cube (want 0/0)"
ml=$(jget "$d0" "d['inventory']['magic_level']"); keys=$(jget "$d0" "d['inventory']['keys']")
[ "$ml" = "0" ] && [ "$keys" = "0" ] \
    || fail "not a fresh new game: magic_level=$ml keys=$keys (want 0/0)"
# The two quest flags beat 3 will flip must start clear, or that test proves nothing.
b94=$(jget "$d0" "d['vars']['nonzero'].get('94',0)"); b253=$(jget "$d0" "d['vars']['nonzero'].get('253',0)")
[ "$b94" = "0" ] && [ "$b253" = "0" ] \
    || fail "quest vars 94/253 not clear at boot: 94=$b94 253=$b253"

# ── Beat 2: the hero walks under injected input (hold 'up' for 60 frames) ──
ctl_headless --fixed-dt 16 --exec "input up 60" --tick 70 --dump-state "$d1" --exit >/dev/null 2>&1 \
    || fail "walk run: non-zero exit ($?)"
x0=$(jget "$d0" "d['hero']['x']"); z0=$(jget "$d0" "d['hero']['z']")
x1=$(jget "$d1" "d['hero']['x']"); z1=$(jget "$d1" "d['hero']['z']")
moved=$(python3 -c "print(abs($x1-$x0)+abs($z1-$z0))")
[ "$moved" -gt 100 ] \
    || fail "hero did not walk under 'input up': moved=$moved units (from $x0,$z0 to $x1,$z1)"

# ── Beat 3: a scenaric trigger fires the hero's Life script and sets quest flags ──
# zonelist cube 0: [0] scenaric Num=1 box=(12288,2048,2560)-(13823,3327,4095).
ctl_headless --fixed-dt 16 --exec "teleport 13055 2687 3327" --tick 12 --dump-state "$d2" --exit >/dev/null 2>&1 \
    || fail "scenaric run: non-zero exit ($?)"
v94=$(jget "$d2" "d['vars']['nonzero'].get('94',0)"); v253=$(jget "$d2" "d['vars']['nonzero'].get('253',0)")
[ "$v94" = "1" ] && [ "$v253" = "1" ] \
    || fail "scenaric zone did not advance quest state: var94=$v94 var253=$v253 (want 1/1)"

# ── Beat 4: walking into the doorway zone changes scene (leave the house) ──
# zonelist cube 0: [4] cube Num=49 box=(11776,768,10240)-(12800,2560,10752).
ctl_headless --fixed-dt 16 --exec "teleport 12288 1664 10496" --tick 20 --dump-state "$d3" --exit >/dev/null 2>&1 \
    || fail "door run: non-zero exit ($?)"
nc=$(jget "$d3" "d['scene']['cube']")
[ "$nc" = "49" ] \
    || fail "did not transition through the door: cube=$nc (want 49)"

pass "house 0/0 fresh, walked $moved units, scenaric set vars 94/253, door -> cube 49"
