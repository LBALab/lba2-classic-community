#!/usr/bin/env bash
# The Aggressive melee attack: that it fires, and that a held press keeps the same blow.
#
# The blow is chosen by MyRnd(3) between GEN_ANIM_COUP_1..3 (17, 18, 19), but only when the draw
# is reached. OBJECT.CPP guards it:
#
#     if ((LastInput & I_ACTION_M) AND (ptrobj->GenAnim != GEN_ANIM_RIEN))
#             break;
#
# and the 1997 comment above it says what that costs: "Ici, il y a un bug, le IF qui suit empeche
# de retirer au hasard une nouvelle anim. Resultat, Twinsen combat toujours avec le meme coup..."
# -- the IF stops a new animation being drawn, so Twinsen always fights with the same blow.
#
# That is original behaviour and docs/BIT_EXACTNESS.md says it stays. This pins it rather than
# fixing it, and it is why the fixture is deterministic despite the draw: the first blow is
# whatever the save's RNG stream yields, and holding keeps it for the whole press.
#
# Asserted as membership plus stability, never as a particular blow. MyRnd draws from the one
# shared stream, so which of the three appears is not this fixture's business and pinning it would
# make every unrelated change upstream of the draw look like a failure here.
#
# No enemy is needed: Twinsen swings in Aggressive whether or not anything is in range. A fixture
# that lands hits on a real enemy is a separate thing and wants an actor confirmed hostile first.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=attack_melee
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

SPACE=44        # I_ACTION_M's default binding
AGGRESSIVE=2

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

run() { # name, exec, ticks
    ctl_headless --load "$FIX" --fixed-dt 16 --exec "$2" --tick "$3"  \
        --dump-state "$out/$1.json" --exit >/dev/null 2>&1 || fail "$1 run: exit $?"
}
gen_anim() { python3 -c "import json;print(json.load(open('$out/$1.json'))['hero']['gen_anim'])"; }

# Aggressive, hands down: no blow.
run idle "skipmodals 1; behaviour $AGGRESSIVE" 60
[ "$(gen_anim idle)" = "0" ] \
    || fail "standing in Aggressive should play no blow, got GenAnim $(gen_anim idle)"

# Aggressive, action held: a blow, and the same one throughout.
run blow_early "skipmodals 1; behaviour $AGGRESSIVE; key $SPACE 120 30" 40
run blow_late  "skipmodals 1; behaviour $AGGRESSIVE; key $SPACE 120 30" 80

early="$(gen_anim blow_early)"; late="$(gen_anim blow_late)"

case "$early" in
    17|18|19) ;;
    *) fail "holding action in Aggressive should play COUP_1/2/3 (17/18/19), got GenAnim $early" ;;
esac

[ "$early" = "$late" ] \
    || fail "a held press must keep the same blow (the 1997 bug this preserves): tick 40 gave $early, tick 80 gave $late"

pass "Aggressive melee fires (GenAnim $early) and a held press keeps that blow from tick 40 to 80"
