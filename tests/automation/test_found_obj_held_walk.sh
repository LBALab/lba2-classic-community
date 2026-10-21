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
# The fix is a retail fix (SOURCES/RETAILFIX.H): the 1997 code has the same slip. So the arms
# with it switched off, each way a player can, must still get stuck: master No, Custom with
# the fix's own row off, and `settings_before fix_`. Each proves the switch reaches the fix site,
# and that off is the behaviour from before the fix.
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips cleanly
# otherwise. Each run works on a temp copy of the save.
# Tier: medium
TESTNAME=found_obj_held_walk
. "$(dirname "$0")/lib.sh"
precheck

CORPUS="$REPO/tests/savegame/corpus/saves/steam_classic_2023"
# Saves where the hero walks freely ahead on load (Bu: 105 units over the window, Spaceship: 709).
SAVES=("Bu" "Spaceship")

hero_xz_at() { # rules save-src tick [extra args...]
    local rules="$1" src="$2" tick="$3" dir
    shift 3
    dir="$(mktemp -d)"
    cp "$src" "$dir/s.LBA"
    mkdir "$dir/user"
    # A user folder per run: the cfg written at exit would otherwise carry one arm's
    # fix_ settings into the next arm's runs.
    LBA2_USER_DIR="$dir/user" ctl_headless --load "$dir/s.LBA" --fixed-dt 16 --exec "${rules}skipmodals 1; godmode 1; input up 400" "$@" \
        --tick "$tick" --dump-state "$dir/state.json" --exit >/dev/null 2>&1
    jget "$dir/state.json" "'%d %d' % (d['hero']['x'], d['hero']['z'])" 2>/dev/null
    rm -rf "$dir"
}

walked() { # rules save-src [extra args...] -> distance walked between ticks 90 and 130
    local rules="$1" src="$2" a b
    shift 2
    a="$(hero_xz_at "$rules" "$src" 90 "$@")"; b="$(hero_xz_at "$rules" "$src" 130 "$@")"
    [ -n "$a" ] && [ -n "$b" ] || { echo ""; return; }
    python3 -c "import sys; a=list(map(int,sys.argv[1].split())); b=list(map(int,sys.argv[2].split())); print(int(((b[0]-a[0])**2+(b[1]-a[1])**2)**.5))" "$a" "$b"
}

tested=0
fails=""
for name in "${SAVES[@]}"; do
    src="$CORPUS/$name.LBA"
    [ -f "$src" ] || continue
    ctl="$(walked "" "$src")"
    pop="$(walked "" "$src" --exec-at 60 "give holomap")"
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
    # Off, each way: the original's stuck walk. Under 50 is what the fix exists to change.
    for rules in "fix_retail 0; " "fix_retail 2; fix_found_obj_walk 0; " "settings_before fix_; "; do
        off="$(walked "$rules" "$src" --exec-at 60 "give holomap")"
        if [ -z "$off" ]; then
            fails="$fails
  $name: dump failed with '$rules'"
        elif [ "$off" -ge 50 ]; then
            fails="$fails
  $name: walked $off units with the fix off ('$rules'), expected the original's stuck walk"
        fi
    done
done

[ "$tested" -gt 0 ] || skip "no usable corpus save (steam_classic_2023 not present?)"
[ -z "$fails" ] || fail "held direction stuck after the found-object popup (#747):$fails"
pass "Forward held through the found-object popup still walks, and stays stuck with the fix off ($tested save(s))"
