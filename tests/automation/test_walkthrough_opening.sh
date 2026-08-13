#!/usr/bin/env bash
# Walkthrough e2e: drive the OPENING of the game through the engine, from a cold
# boot, with no savegame. A savegame would teleport past the game; this drives
# the real thing and asserts the engine's own reaction at each beat.
#
# The four beats mirror the start of any Twinsen's Odyssey walkthrough:
#   1. A new game starts you in Twinsen's house (island 0, cube 0), fresh.
#   2. The house opening is the scene's own choreography: the hero walks it out
#      himself, and the player has no control yet.
#   3. Walking into the doorway leaves the house for the next scene (cube 49).
#   4. Outside, that scene's script advances quest state and the hero answers input.
#
# Everything is driven with stock console verbs (input / teleport) under
# --fixed-dt for determinism. Zone coordinates were read off the `zonelist`
# console command (cube 0): the cube-change zone to 49 at [4].
#
# Four things about the opening that the beats are built around, each measured
# rather than assumed. The first three are each a way for a test written in here
# to prove nothing at all, which is what the earlier version of this file did:
#
#   * The opening Life script stamps the new game on the FIRST simulated frame:
#     game vars 94 (FLAG_DINO_VOYAGE) and 253 (FLAG_CHAPTER) go to 1 before any
#     tick a test can observe. No cold-boot test can ever watch those flip, so
#     they are asserted here as the new-game stamp, never used as a probe.
#   * --exec runs on tick 0, ahead of the opening script placing the hero, so a
#     tick-0 `teleport` is silently undone. Every teleport below is --exec-at 1.
#   * The house opening owns the hero: he walks it out on his own and injected
#     input moves him not at all, so the walk assertion lives outside, in beat 4.
#   * A house run past ~200 ticks walks into the opening dialogue and blocks on
#     the modal. All house runs here stay well short of it.
#
# Nothing in the house advances quest state on its own: a teleport into any of
# the cube's scenaric boxes latches the hero's ZoneSce and stops there, and the
# giver boxes need a grounded action edge no headless run has managed. The door
# is the opening's first real quest event, which is why beat 4 waits for it.
#
# Local-only: needs retail data (LBA2_GAME_DIR) and a build. No save required.
TESTNAME=walkthrough_opening
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
d0="$tmp/0.json"; d1="$tmp/1.json"; d2="$tmp/2.json"
d3="$tmp/3.json"; d4="$tmp/4.json"

# A user directory of this run's own. The beats below are distances the hero
# covers per simulated tick, and lba2.cfg's FixedTimestep decides how many of
# those a tick budget buys, so a shared config carrying another run's throttle
# setting shortens the walk and reads as a broken engine. Isolating also keeps
# the run off the developer's own saves and settings entirely.
export LBA2_USER_DIR="$tmp/user"

DOOR="teleport 12288 1664 10496"

# ── Beat 1: a cold boot lands in Twinsen's house, in a fresh new-game state ──
ctl_headless --fixed-dt 16 --tick 5 --dump-state "$d0" --exit >/dev/null 2>&1 \
    || fail "cold boot: non-zero exit ($?)"
isl=$(jget "$d0" "d['scene']['island']"); cube=$(jget "$d0" "d['scene']['cube']")
[ "$isl" = "0" ] && [ "$cube" = "0" ] \
    || fail "cold boot not in the house: island=$isl cube=$cube (want 0/0)"
ml=$(jget "$d0" "d['inventory']['magic_level']"); keys=$(jget "$d0" "d['inventory']['keys']")
[ "$ml" = "0" ] && [ "$keys" = "0" ] \
    || fail "not a fresh new game: magic_level=$ml keys=$keys (want 0/0)"
# The new-game stamp the opening script writes on frame one. Asserted so a boot
# that skipped it is caught here rather than mistaken for quest progress later.
b94=$(jget "$d0" "d['vars']['nonzero'].get('94',0)"); b253=$(jget "$d0" "d['vars']['nonzero'].get('253',0)")
[ "$b94" = "1" ] && [ "$b253" = "1" ] \
    || fail "new game not stamped by the opening script: var94=$b94 var253(chapter)=$b253 (want 1/1)"
# The flags beat 4 watches must start clear, or that test proves nothing.
b164=$(jget "$d0" "d['vars']['nonzero'].get('164',0)"); b165=$(jget "$d0" "d['vars']['nonzero'].get('165',0)")
[ "$b164" = "0" ] && [ "$b165" = "0" ] \
    || fail "quest vars 164/165 not clear at boot: 164=$b164 165=$b165"

# ── Beat 2: the house opening runs itself, and the player has no control yet ──
# The hero walks his opening track with no input at all, so a displacement test
# in here would pass on the script alone. Both halves are asserted: he moves,
# and the same run with 'up' held is identical, which is what sends the real
# walk assertion outside to beat 4.
ctl_headless --fixed-dt 16 --tick 70 --dump-state "$d1" --exit >/dev/null 2>&1 \
    || fail "house idle run: non-zero exit ($?)"
ctl_headless --fixed-dt 16 --exec-at 1 "input up 60" --tick 70 --dump-state "$d2" --exit >/dev/null 2>&1 \
    || fail "house input run: non-zero exit ($?)"
x0=$(jget "$d0" "d['hero']['x']"); z0=$(jget "$d0" "d['hero']['z']")
x1=$(jget "$d1" "d['hero']['x']"); z1=$(jget "$d1" "d['hero']['z']")
x2=$(jget "$d2" "d['hero']['x']"); z2=$(jget "$d2" "d['hero']['z']")
scripted=$(python3 -c "print(abs($x1-$x0)+abs($z1-$z0))")
[ "$scripted" -gt 300 ] \
    || fail "the opening did not walk the hero: moved=$scripted units (from $x0,$z0 to $x1,$z1)"
[ "$x1" = "$x2" ] && [ "$z1" = "$z2" ] \
    || fail "'input up' now moves the hero during the house opening ($x1,$z1 vs $x2,$z2): the walk beat can move back in here"

# ── Beat 3: walking into the doorway zone changes scene (leave the house) ──
# zonelist cube 0: [4] cube Num=49 box=(11776,768,10240)-(12800,2560,10752).
ctl_headless --fixed-dt 16 --exec-at 1 "$DOOR" --tick 120 --dump-state "$d3" --exit >/dev/null 2>&1 \
    || fail "door run: non-zero exit ($?)"
nc=$(jget "$d3" "d['scene']['cube']")
[ "$nc" = "49" ] \
    || fail "did not transition through the door: cube=$nc (want 49)"

# ── Beat 4: the new scene advances quest state, and the hero answers input ──
# Cube 49's own Life script sets game vars 164 and 165 on arrival, the first
# quest flags of the run that a cold boot did not already hold.
v164=$(jget "$d3" "d['vars']['nonzero'].get('164',0)"); v165=$(jget "$d3" "d['vars']['nonzero'].get('165',0)")
[ "$v164" = "1" ] && [ "$v165" = "1" ] \
    || fail "leaving the house did not advance quest state: var164=$v164 var165=$v165 (want 1/1)"
ctl_headless --fixed-dt 16 --exec-at 1 "$DOOR" --exec-at 10 "input up 100" --tick 120 \
    --dump-state "$d4" --exit >/dev/null 2>&1 \
    || fail "outside walk run: non-zero exit ($?)"
x3=$(jget "$d3" "d['hero']['x']"); z3=$(jget "$d3" "d['hero']['z']")
x4=$(jget "$d4" "d['hero']['x']"); z4=$(jget "$d4" "d['hero']['z']")
walked=$(python3 -c "print(abs($x4-$x3)+abs($z4-$z3))")
[ "$walked" -gt 500 ] \
    || fail "hero did not walk under 'input up' outside: moved=$walked units (from $x3,$z3 to $x4,$z4)"

pass "house 0/0 fresh, opening walked $scripted units on its own, door -> cube 49 set vars 164/165, then 'up' walked $walked units"
