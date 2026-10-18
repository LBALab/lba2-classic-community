#!/usr/bin/env bash
# A direction held through the found-object popup must still move the hero afterwards (#747).
#
# DoFoundObj (INVENT.CPP) turns the hero to the camera and back by setting his body anim with
# ObjectInitAnim, which leaves GenAnim naming the anim he had on entry. The manual move code
# trusts GenAnim: with the same direction still held, InitAnim(GEN_ANIM_MARCHE) finds it "already
# playing" and returns. Before the fix the hero stood in the idle body with Forward held (no
# movement) and turned at the walking rate, until every direction was released.
#
# Test: hold Forward, give an item (opens the popup; skipmodals closes it at once), and measure how
# far the hero walks over a window after it. A run without the popup must walk too, else the save
# blocks the route and proves nothing.
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips cleanly
# otherwise. Each run works on a temp copy of the save.
TESTNAME=found_obj_held_walk
. "$(dirname "$0")/lib.sh"
precheck

CORPUS="$REPO/tests/savegame/corpus/saves/steam_classic_2023"
# Saves where the hero walks freely ahead on load (Bu: 105 units over the window, Spaceship: 709).
SAVES=("Bu" "Spaceship")

hero_xz_at() { # save-src tick [extra args...]
    local src="$1" tick="$2" dir
    shift 2
    dir="$(mktemp -d)"
    cp "$src" "$dir/s.LBA"
    ctl_headless --load "$dir/s.LBA" --fixed-dt 16 --exec "skipmodals 1; godmode 1; input up 400" "$@" \
        --tick "$tick" --dump-state "$dir/state.json" --exit >/dev/null 2>&1
    jget "$dir/state.json" "'%d %d' % (d['hero']['x'], d['hero']['z'])" 2>/dev/null
    rm -rf "$dir"
}

walked() { # save-src [extra args...] -> distance walked between ticks 90 and 130
    local src="$1" a b
    shift
    a="$(hero_xz_at "$src" 90 "$@")"; b="$(hero_xz_at "$src" 130 "$@")"
    [ -n "$a" ] && [ -n "$b" ] || { echo ""; return; }
    python3 -c "import sys; a=list(map(int,sys.argv[1].split())); b=list(map(int,sys.argv[2].split())); print(int(((b[0]-a[0])**2+(b[1]-a[1])**2)**.5))" "$a" "$b"
}

tested=0
fails=""
for name in "${SAVES[@]}"; do
    src="$CORPUS/$name.LBA"
    [ -f "$src" ] || continue
    ctl="$(walked "$src")"
    pop="$(walked "$src" --exec-at 60 "give holomap")"
    [ -n "$ctl" ] && [ -n "$pop" ] || { fails="$fails
  $name: dump failed (control='$ctl' popup='$pop')"; continue; }
    if [ "$ctl" -lt 50 ]; then
        fails="$fails
  $name: control run walked only $ctl units -- bad fixture"
        continue
    fi
    tested=$((tested + 1))
    if [ "$pop" -lt 50 ]; then
        fails="$fails
  $name: walked $pop units after the popup with Forward held (control: $ctl)"
    fi
done

[ "$tested" -gt 0 ] || skip "no usable corpus save (steam_classic_2023 not present?)"
[ -z "$fails" ] || fail "held direction stuck after the found-object popup (#747):$fails"
pass "Forward held through the found-object popup still walks ($tested save(s))"
