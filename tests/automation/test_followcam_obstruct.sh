#!/usr/bin/env bash
# With cam_obstruct on, the Auto camera tilts over what stands behind the hero as an extra that eases away again, and the player's own pitch is never rewritten.
#
# The classic camera asks its obstruction search, once per recentre, how steeply it has to look
# down to clear the terrain and scenery behind the hero, and the answer is written into the
# pitch. Under the Auto camera nothing takes it away again: the hero steps out of a building,
# the camera looks nearly straight down over the roof, and it goes on looking down wherever he
# walks and however the player turns it.
#
# cam_obstruct (a prototype, off by default) has the Auto camera ask the same search on every
# update and carry the answer as an extra added to the pitch for the frame only. The route here
# is the Tavern's door: out of the building with it right behind him, then the player orbits the
# camera half a turn into the open and back again. The extra should be there on arrival, gone
# with the camera in the open, and back when the camera is; the player's own pitch should not
# move at all; and the hero should be in view on every frame, with no recentre to put him there.
#
# The same route with the setting off is the control. If the search wrote nothing there, or the
# hero was never hidden, every claim about the setting would be true of a place with nothing in
# the way.
#
# A second arrival, without cam_scene_carry, starts the extra from nothing with the building
# already in the way, so the hero is hidden while it rises. That is the one moment the classic
# recentre used to cut in and take the player's pan with it. The clearance should hurry and
# the recentre stay out.
#
# A third place is one the search is wrong about. Walking west out of cube 42 into cube 43 with
# the camera panned, the hero is drawn fully hidden a few frames before the seam while the search
# asks for less tilt, not more. There the render's word is taken: the tilt is raised and held,
# and the recentre, which would take the pan, stays out.
#
# Needs cam_zone_skip, so the shots over the door and the seam do not take the view.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_obstruct
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

off="$(mktemp)"; on="$(mktemp)"; rising="$(mktemp)"; seam_off="$(mktemp)"; seam_on="$(mktemp)"
trap 'rm -f "$off" "$on" "$rising" "$seam_off" "$seam_on"' EXIT

route() { # <log> <cam_obstruct> <cam_scene_carry> <rest of the timeline...>
    local log="$1" obstruct="$2" carry="$3"
    shift 3
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; cam_zone_skip 1; cam_scene_carry $carry; cam_obstruct $obstruct; camtrace 1; cubetrace 1" \
        --exec-at 20 "cube 3" \
        --exec-at 60 "teleport 7100 1024 15600" \
        "$@" --exit > "$log" 2>&1 || fail "run failed: exit $?"
    grep -q "\[cube\] island [0-9]*: 3 -> 42" "$log" || fail "the hero never came out of the Tavern"
}

# Half a turn round into the open, a pause, and half a turn back.
orbit=(--exec-at 100 "camnudge 30 0 68" --exec-at 220 "camnudge -30 0 68" --tick 340)

# outdoors <log> -- over the frames after the building, one line of:
#   own       the pitch he went indoors with
#   lo hi     the lowest and highest pitch since
#   first     the extra on the arrival frame
#   least     the smallest extra, and
#   leastpan  how far round the camera was panned on that frame (0..2048)
#   last      the extra at the end
#   jump      the largest change in the extra from one frame to the next, after arrival
#   recentred frames on which the hidden-hero recentre fired
#   hurried   frames on which he was drawn hidden with the clearance still rising
#   hidden    frames on which he was drawn fully hidden and left so
#   forced    whether the obstruction search's flag was ever raised
#   endpitch  the pitch at the end
outdoors() {
    cam_tsv "$1" | awk '
        NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
        $col["ext"] == 0 { indoors = 1; own = $col["alpha"]; next }
        indoors {
            a = $col["alpha"]; o = $col["obs"]; h = $col["hid"]
            pan = $col["add"]; if (pan > 2048) pan = 4096 - pan
            if (!n) { lo = a; hi = a; first = o; least = o; leastpan = pan }
            else { d = o - prev; if (d < 0) d = -d; if (d > jump) jump = d }
            if (a < lo) lo = a
            if (a > hi) hi = a
            if (o < least || (o == least && pan > leastpan)) { least = o; leastpan = pan }
            if (h == 3) recentred++
            else if (h == 2) hurried++
            else if (h == 1) hidden++
            if ($col["forced"] > forced) forced = $col["forced"]
            prev = o; last = o; endpitch = a; n++
        }
        END { print own + 0, lo + 0, hi + 0, first + 0, least + 0, leastpan + 0, last + 0, jump + 0, recentred + 0, hurried + 0, hidden + 0, forced + 0, endpitch + 0 }'
}

route "$off" 0 1 "${orbit[@]}"
route "$on" 1 1 "${orbit[@]}"

read -r off_own _ off_hi _ _ _ _ _ off_recentred _ _ _ off_endpitch <<< "$(outdoors "$off")"
read -r on_own on_lo on_hi on_first on_least on_leastpan on_last on_jump on_recentred on_hurried on_hidden on_forced _ <<< "$(outdoors "$on")"

# The control: without the setting this door does what the setting is for.
[ "$off_recentred" -ge 1 ] && [ "$off_hi" -gt 600 ] && [ "$off_endpitch" = "$off_hi" ] \
    || fail "with the setting off the recentre fired $off_recentred time(s), the pitch reached $off_hi from $off_own and ended at $off_endpitch: nothing here for the setting to change"

[ "$on_lo" = "$on_own" ] && [ "$on_hi" = "$on_own" ] \
    || fail "with cam_obstruct on the player's pitch moved between $on_lo and $on_hi, having been $on_own"
[ "$on_first" -ge 300 ] \
    || fail "with cam_obstruct on the camera arrived with $on_first units of extra pitch over a building the classic search tilts $((off_hi - off_own)) for"
[ "$on_least" -eq 0 ] && [ "$on_leastpan" -ge 1500 ] \
    || fail "with cam_obstruct on the extra pitch never went below $on_least (camera panned $on_leastpan units round at that point): it is not given back in the open"
[ "$on_last" -ge 300 ] \
    || fail "with cam_obstruct on the extra pitch was $on_last with the camera back behind the hero and the building behind it"
[ "$on_jump" -le 200 ] \
    || fail "with cam_obstruct on the extra pitch changed by $on_jump units in one frame after arrival: a cut, not an ease"
[ "$on_recentred" -eq 0 ] && [ "$on_hurried" -eq 0 ] && [ "$on_hidden" -eq 0 ] \
    || fail "with cam_obstruct on the hero was drawn hidden on $((on_hurried + on_hidden)) frame(s) and recentred on $on_recentred"
[ "$on_forced" -eq 0 ] \
    || fail "with cam_obstruct on the obstruction search still raised its forced flag"

# The arrival that starts from nothing, and nothing after it.
route "$rising" 1 0 --tick 160

read -r up_own up_lo up_hi up_first _ _ up_last _ up_recentred up_hurried up_hidden _ _ <<< "$(outdoors "$rising")"

[ "$up_first" -lt 300 ] && [ "$up_hurried" -ge 1 ] \
    || fail "without cam_scene_carry the extra started at $up_first and the hero was hidden while it rose on $up_hurried frame(s): the clearance was never caught on its way up"
[ "$up_recentred" -eq 0 ] && [ "$up_hidden" -eq 0 ] \
    || fail "with the clearance still rising the hero was recentred on $up_recentred frame(s) and left hidden on $up_hidden"
[ "$up_last" -ge 300 ] && [ "$up_lo" = "$up_own" ] && [ "$up_hi" = "$up_own" ] \
    || fail "the rising extra ended at $up_last with the pitch between $up_lo and $up_hi, having been $up_own"

# The seam. The walk starts at frame 110 with the pan the nudge left, and crosses about two
# seconds later.
seam() { # <log> <cam_obstruct>
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; cam_zone_skip 1; cam_scene_carry 1; cam_obstruct $2; camtrace 1; cubetrace 1" \
        --exec-at 20 "cube 42" \
        --exec-at 60 "teleport 5000 250 19200 3072" \
        --exec-at 70 "camnudge 20 0 20" \
        --exec-at 110 "input up 260" \
        --tick 420 --exit > "$1" 2>&1 || fail "seam run failed: exit $?"
    grep -q "\[cube\] island [0-9]*: 42 -> 43" "$1" || fail "the hero never crossed from cube 42 into cube 43"
}

# crossing <log> -- "<pan before the walk> <pan at the end> <frames recentred> <frames hidden with
# the clearance answering>", over the walk.
crossing() {
    cam_tsv "$1" | awk '
        NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
        NR - 1 == 105 { before = $col["add"] }
        NR - 1 > 105 {
            if ($col["hid"] == 3) recentred++
            else if ($col["hid"] == 2) hurried++
            after = $col["add"]
        }
        END { print before + 0, after + 0, recentred + 0, hurried + 0 }'
}

seam "$seam_off" 0
seam "$seam_on" 1

read -r soff_before soff_after soff_recentred _ <<< "$(crossing "$seam_off")"
read -r son_before son_after son_recentred son_hurried <<< "$(crossing "$seam_on")"

[ "$soff_before" -ne 0 ] && [ "$soff_recentred" -ge 1 ] && [ "$soff_after" -eq 0 ] \
    || fail "with the setting off the walk to the seam began with pan $soff_before, recentred $soff_recentred time(s) and ended with pan $soff_after: no recentre here for the setting to leave out"
[ "$son_recentred" -eq 0 ] && [ "$son_hurried" -ge 1 ] \
    || fail "with cam_obstruct on the walk to the seam recentred $son_recentred time(s), with the hero hidden and answered on $son_hurried frame(s)"
[ "$son_after" = "$son_before" ] \
    || fail "with cam_obstruct on the pan went from $son_before to $son_after on the walk to the seam"

pass "out of the Tavern the pitch went from $off_own to $off_hi and stayed there with the setting off; with it on the pitch stayed $on_own, the extra was $on_first on arrival, 0 with the camera $on_leastpan units round, $on_last back behind him, at most $on_jump a frame, and he was never hidden; starting from $up_first it reached $up_last with him hidden on $up_hurried frame(s) and no recentre; at the seam the pan $soff_before was recentred away with it off and kept with it on, over $son_hurried hidden frame(s)"
