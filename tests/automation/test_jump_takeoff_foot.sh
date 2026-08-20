#!/usr/bin/env bash
# The running jump takes off from whichever foot is down, and the press frame decides which.
#
# Walking in Sporty arms the jump each time the walk animation reaches its first frame
# (Jumping = 1024). Pressing action while armed picks the take-off from the foot flags the walk
# animation itself sets, in OBJECT.CPP:
#
#     if (ptrobj->WorkFlags & LEFT_JUMP)       Jumping = DO_LEFT_JUMP;
#     else if (ptrobj->WorkFlags & RIGHT_JUMP) Jumping = DO_RIGHT_JUMP;
#     else                                     Jumping = DO_NORMAL_JUMP;
#
# and LEFT_JUMP / RIGHT_JUMP are turned on at authored keyframes (FICHE.CPP, F_LEFT_JUMP and
# F_RIGHT_JUMP). So the same press, twenty polls apart, is a different jump. This is the frame
# exactness the speedrunning community depends on, and nothing tested it.
#
# Three press offsets against one walk give all three take-offs:
#
#     GEN_ANIM_SAUTE          14   neither foot flag set yet
#     GEN_ANIM_SAUTE_GAUCHE   25   left foot down
#     GEN_ANIM_SAUTE_DROIT    26   right foot down
#
# Asserted as three distinct outcomes from three press times, and as the exact set, because none
# of this involves MyRnd: the foot is decided by animation frame, which is deterministic under
# --fixed-dt. A change that flattened the jump to one variant, or that shifted the walk's keyframes,
# moves at least one of these.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=jump_takeoff_foot
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

UP=82; SPACE=44; SPORTY=1
SAUTE=14; SAUTE_GAUCHE=25; SAUTE_DROIT=26

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# Walk from tick 0, press action `polls` in, and read the take-off at tick 120.
#
# Reports through TAKEOFF and is called directly, never as `x=$(jump_at ...)`: `fail` ends the
# shell it runs in, which inside $( ) is only the subshell, so a run that crashed would come back
# as an empty string and be reported as the wrong take-off rather than as a crash.
TAKEOFF=""
jump_at() {
    local at="$1"
    ctl_headless --load "$FIX" --fixed-dt 16 \
        --exec "skipmodals 1; behaviour $SPORTY; key $UP 900 20; key $SPACE 12 $at" \
        --tick 120 --dump-state "$out/$at.json" --exit >/dev/null 2>&1 || fail "press at $at: exit $?"
    TAKEOFF=$(python3 -c "import json;print(json.load(open('$out/$at.json'))['hero']['gen_anim'])")
    [ -n "$TAKEOFF" ] || fail "press at $at: no hero animation in the dump"
}

jump_at 60; early="$TAKEOFF"   # before either foot flag is up
jump_at 80; left="$TAKEOFF"
jump_at 100; right="$TAKEOFF"

[ "$early" = "$SAUTE" ] \
    || fail "pressing before a foot is down should give the plain jump ($SAUTE), got $early"
[ "$left" = "$SAUTE_GAUCHE" ] \
    || fail "pressing on the left foot should give SAUTE_GAUCHE ($SAUTE_GAUCHE), got $left"
[ "$right" = "$SAUTE_DROIT" ] \
    || fail "pressing on the right foot should give SAUTE_DROIT ($SAUTE_DROIT), got $right"

# The point of the fixture: the same action, at three times, is three jumps.
[ "$early" != "$left" ] && [ "$left" != "$right" ] && [ "$early" != "$right" ] \
    || fail "the three press times collapsed to the same take-off: $early $left $right"

pass "the press frame picks the take-off foot: $early plain, $left left, $right right, twenty polls apart"
