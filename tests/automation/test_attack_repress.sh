#!/usr/bin/env bash
# Holding the attack against re-pressing it: the combo the 1997 comment describes.
#
# OBJECT.CPP guards the MyRnd(3) blow draw with
#
#     if ((LastInput & I_ACTION_M) AND (ptrobj->GenAnim != GEN_ANIM_RIEN))
#             break;
#
# so while the key is down AND a blow is already playing, no new blow is drawn. The comment above
# it: "Ici, il y a un bug, le IF qui suit empeche de retirer au hasard une nouvelle anim. Resultat,
# Twinsen combat toujours avec le meme coup..." -- the IF stops a new animation being drawn, so
# Twinsen always fights with the same blow.
#
# Both halves of the guard are pinned here:
#
#   held      one press, sampled across a long window: the same blow throughout. The draw is
#             never re-reached because GenAnim never returns to RIEN.
#   re-pressed taps with gaps: GenAnim returns to RIEN between them, which is the draw being
#             re-reached. That is the state the guard tests, so observing it is observing the
#             other arm.
#
# What is deliberately NOT asserted is that re-pressing yields a DIFFERENT blow. MyRnd(3) can draw
# the same one twice, so that assertion would fail about one run in a hundred, and the RNG is a
# single shared stream so seeding it here would move every draw downstream. Structure is
# deterministic; the value is not (docs/plan/INPUT_PLAN.md, increment 1).
#
# Original behaviour, preserved rather than fixed: docs/BIT_EXACTNESS.md.
#
# Undisturbed on purpose -- no enemy. Being struck plays a hit reaction that interrupts the blow,
# after which a held key does draw again; that is the interruption showing through, not the guard
# failing, and mixing it in would make this fixture measure two things at once.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=attack_repress
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

SPACE=44; AGGRESSIVE=2
out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# Reports through BLOW and is called directly, never as `x=$(sample ...)`: `fail` ends the shell
# it runs in, which inside $( ) is only the subshell, so a run that crashed would come back as an
# empty string and be reported as the wrong blow rather than as a crash.
BLOW=""
sample() { # name, exec, tick -> sets BLOW to the hero's GenAnim
    ctl_headless --load "$FIX" --fixed-dt 16 --exec "$2" --tick "$3" \
        --dump-state "$out/$1$3.json" --exit >/dev/null 2>&1 || fail "$1 at tick $3: exit $?"
    BLOW=$(python3 -c "import json;print(json.load(open('$out/$1$3.json'))['hero']['gen_anim'])")
    [ -n "$BLOW" ] || fail "$1 at tick $3: no hero animation in the dump"
}

# --- held: one press, no chance to redraw -----------------------------------
HELD="skipmodals 1; behaviour $AGGRESSIVE; key $SPACE 900 30"
held=""
for t in 50 80 110 140; do sample held "$HELD" $t; held="$held $BLOW"; done

first=$(echo $held | cut -d' ' -f1)
case "$first" in
    17|18|19) ;;
    *) fail "a held press should be playing a blow by tick 50, got GenAnim $first" ;;
esac
for g in $held; do
    [ "$g" = "$first" ] || fail "a held press must not redraw the blow: samples were$held"
done

# --- re-pressed: the draw is reached again ----------------------------------
# Taps of 40 polls with 40-poll gaps. The gaps are the point: GenAnim falling back to RIEN is
# exactly the condition the guard tests, so seeing it proves the other arm is reachable.
TAPS="skipmodals 1; behaviour $AGGRESSIVE; key $SPACE 40 30; key $SPACE 40 110; key $SPACE 40 190; key $SPACE 40 270"
seen_idle=0; seen_blow=0
for t in 50 70 90 110 130 150; do
    sample taps "$TAPS" $t
    [ "$BLOW" = "0" ] && seen_idle=1
    case "$BLOW" in 17|18|19) seen_blow=1 ;; esac
done

[ "$seen_blow" = "1" ] || fail "re-pressed taps never produced a blow"
[ "$seen_idle" = "1" ] \
    || fail "re-pressed taps never returned to idle, so the draw was never re-reached"

pass "held keeps one blow (GenAnim $first across 4 samples); re-pressing returns to idle between taps, which is where the draw is reached again"
