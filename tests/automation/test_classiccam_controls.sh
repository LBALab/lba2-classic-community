#!/usr/bin/env bash
# The classic camera's own controls: quarter turns, recentre, and the two view presets.
#
# `FollowCamera 0` is the original game's camera and had no coverage at all, which is the wrong way
# round: it is the behaviour the Auto camera is a departure from, and in most of what the camera
# work turned up it was the classic path that was already right. Pinning it makes it a reference
# rather than an assumption.
#
# Two things are being asserted. That each control does what it is for, and that BetaCam keeps its
# stated relationship to the hero's facing while they do it:
#
#     BetaCam = (2048 - (AddBetaCam + heroBeta)) & 4095
#
# That rule is the one every camera bug so far has come from a writer quietly breaking, so it is
# worth an assertion of its own rather than being left implicit in a expected angle.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=classiccam_controls
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

K_NUMPAD_PLUS=87  # I_CAMERA_LEVEL_PLUS's default keyboard binding
K_NUMPAD_MINUS=86 # I_CAMERA_LEVEL_MOINS

# The camera-level controls run inside GereExtKeys, which returns immediately in classic mode
# unless a raw key is down, so they are driven with `key` rather than `input`: injected funnel
# flags alone never reach them. The turn and recentre controls are read from the funnel mask and
# take `input`. One tick each, because injected input does not go through the press debounce and a
# longer hold would rotate once per tick.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cam_follow 0; camtrace 1" \
    --exec-at 30 "input 0x400000 1" \
    --exec-at 60 "key $K_NUMPAD_PLUS 2" \
    --exec-at 90 "key $K_NUMPAD_MINUS 2" \
    --exec-at 120 "input 0x200 1" \
    --tick 160 --exit > "$log" 2>&1 || fail "run failed: exit $?"

# The rule holds where the camera is re-aimed at the hero, which is not the same as always: the
# classic camera keeps whatever angle it has until a control re-aims it, and a save restores an
# angle that satisfies no such relationship. The recentre is the moment it must hold, so it is
# checked on the settled frame after one rather than swept over the run.
read -r hero beta < <(cam_tsv "$log" | awk 'END { print $1, $3 }')
want=$(( (2048 - hero) % 4096 ))
[ "$want" -lt 0 ] && want=$(( want + 4096 ))
[ "$beta" = "$want" ] \
    || fail "after recentre BetaCam is $beta, not the $want that sitting behind a hero facing $hero means"

# after <field> <row-of-first-change-onwards> is fiddly to express; take the distinct values each
# field passes through instead, which is what these controls produce: a small sequence of steps.
# Concatenating the empty string forces a string comparison. awk compares an uninitialised
# variable numerically against a numeric-looking field, so a leading 0 would test equal to nothing
# and be dropped from the sequence -- silently, and exactly where a control starting from rest is
# most interesting.
seq_of() { cam_col "$log" "$1" | awk '$1 "" != p "" { print $1; p = $1 }' | tr '\n' ' '; }

add_seq="$(seq_of add)"
vue_seq="$(seq_of vue)"

# Quarter turn then recentre: the pan visits 1024 and comes back to 0.
case " $add_seq " in
    *" 1024 "*) ;;
    *) fail "camera cycle did not turn the pan a quarter (AddBetaCam went: $add_seq)" ;;
esac
add_last="$(printf '%s' "$add_seq" | awk '{ print $NF }')"
[ "$add_last" = "0" ] \
    || fail "recentre left the pan at $add_last rather than 0 (AddBetaCam went: $add_seq)"

# The level controls pick from an authored table rather than tilting freely, so elevation and
# distance are a function of the view index: every frame sharing a view index shares its angle and
# its distance, and the two indices differ. Stated that way rather than as a round trip back to the
# starting values, because a save restores an elevation that is not any preset (315 here, against
# the table's 300 and 530) and no amount of pressing the control returns to it.
case " $vue_seq " in
    *" 1 "*) ;;
    *) fail "camera level did not change the view preset (VueCamera went: $vue_seq)" ;;
esac

# Each contiguous stretch at one view index, with the elevation and distance it settled on. The
# settled value rather than every frame: the control fires and CameraCenter applies the preset
# within the same frame, so the transition frame reports an intermediate elevation that belongs to
# neither preset.
runs="$(cam_tsv "$log" | awk '
    NR == 1 { next }
    { if (NR > 2 && $14 != cur) print cur, a, d
      cur = $14; a = $15; d = $16 }
    END { print cur, a, d }')"

steps="$(printf '%s\n' "$runs" | wc -l)"
[ "$steps" -ge 3 ] \
    || fail "the view index did not step away and back (saw: $(printf '%s' "$runs" | tr '\n' '/'))"

read -r v_first a_first d_first < <(printf '%s\n' "$runs" | head -1)
read -r v_mid a_mid d_mid < <(printf '%s\n' "$runs" | sed -n '2p')
read -r v_last a_last d_last < <(printf '%s\n' "$runs" | tail -1)

if [ "$a_mid" = "$a_first" ] || [ "$d_mid" = "$d_first" ]; then
    fail "changing the view preset left elevation or distance alone ($a_first/$d_first then $a_mid/$d_mid)"
fi

if [ "$v_last" != "$v_first" ] || [ "$a_last" != "$a_first" ] || [ "$d_last" != "$d_first" ]; then
    fail "returning to view $v_first gave $a_last/$d_last rather than the $a_first/$d_first it held before"
fi

views=$steps

pass "AddBetaCam walked $add_seq, recentre landed BetaCam on $beta, and the presets stepped $a_first/$d_first -> $a_mid/$d_mid and back"
