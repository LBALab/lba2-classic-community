#!/usr/bin/env bash
# With cam_obstruct on, the Auto camera tilts over what stands behind the hero as an extra that eases away again, and the player's own pitch is never rewritten.
#
# The classic camera asks its obstruction search, once per recentre, how steeply it has to look
# down to clear the terrain and scenery behind the hero, and the answer is written into the
# pitch. Under the Auto camera nothing takes it away again: the hero steps out of a building,
# the camera looks nearly straight down over the roof, and it goes on looking down wherever he
# walks and however the player turns it.
#
# cam_obstruct (on by default) has the Auto camera ask the same search on every
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
# the recentre stay out. How hard it hurries is cam_obstruct_hurry, which ships at 10, no hurry
# at all. The Tavern runs set it to 6, so that there is a hurry to see, and the same arrival
# with it at 1, a cut, has to move further in a frame than that.
#
# A third place is one the search is wrong about. Walking west out of cube 42 into cube 43 with
# the camera panned, the hero is drawn fully hidden a few frames before the seam while the search
# asks for less tilt, not more. There the render's word is taken: the tilt is raised and held,
# and the recentre, which would take the pan, stays out.
#
# The last is the box of a shot that has fired. The search is not asked there, since the place
# is the author's to judge, but a hero drawn hidden still has to be answered: with the recentre
# stood down and the tilt held at nothing he was left hidden, by the Dome of the Slate for over
# a second at a time. The Tavern's door stands in for it: its shot fires on arrival, and the
# camera is then orbited into the building.
#
# Needs cam_zone_skip, so the shots over the door and the seam do not take the view.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=followcam_obstruct
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

off="$(mktemp)"; on="$(mktemp)"; rising="$(mktemp)"; cut="$(mktemp)"; seam_off="$(mktemp)"; seam_on="$(mktemp)"; inbox="$(mktemp)"
trap 'rm -f "$off" "$on" "$rising" "$cut" "$seam_off" "$seam_on" "$inbox"' EXIT

route() { # <log> <cam_obstruct> <cam_scene_carry> <rest of the timeline...>
    local log="$1" obstruct="$2" carry="$3"
    shift 3
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_zone_skip 1; cam_scene_carry $carry; cam_obstruct $obstruct; cam_obstruct_hurry 6; camtrace 1; cubetrace 1" \
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

read -r up_own up_lo up_hi up_first _ _ up_last up_jump up_recentred up_hurried up_hidden _ _ <<< "$(outdoors "$rising")"

[ "$up_first" -lt 300 ] && [ "$up_hurried" -ge 1 ] \
    || fail "without cam_scene_carry the extra started at $up_first and the hero was hidden while it rose on $up_hurried frame(s): the clearance was never caught on its way up"
[ "$up_recentred" -eq 0 ] && [ "$up_hidden" -eq 0 ] \
    || fail "with the clearance still rising the hero was recentred on $up_recentred frame(s) and left hidden on $up_hidden"
[ "$up_last" -ge 300 ] && [ "$up_lo" = "$up_own" ] && [ "$up_hi" = "$up_own" ] \
    || fail "the rising extra ended at $up_last with the pitch between $up_lo and $up_hi, having been $up_own"

# The same arrival with the hurry turned up to a cut.
route "$cut" 1 0 --exec-at 1 "cam_obstruct_hurry 1" --tick 160

read -r _ _ _ _ _ _ cut_last cut_jump cut_recentred _ _ _ _ <<< "$(outdoors "$cut")"

[ "$cut_jump" -gt "$up_jump" ] && [ "$cut_last" -ge 300 ] && [ "$cut_recentred" -eq 0 ] \
    || fail "with cam_obstruct_hurry 1 the extra moved at most $cut_jump units in a frame against $up_jump at 6, ended at $cut_last and recentred $cut_recentred time(s): the setting does not reach the hurry"

# The seam. The walk starts at frame 110 with the pan the nudge left, and crosses about two
# seconds later.
seam() { # <log> <cam_obstruct>
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_zone_skip 1; cam_scene_carry 1; cam_obstruct $2; camtrace 1; cubetrace 1" \
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

# In the box of a shot that has fired. With cam_zone_skip off the zone over the Tavern's door
# takes the view as the hero steps out, and he stays in its box while the camera is orbited round
# into the building.
ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
    --exec "cam_follow 1; cam_hold_angle 1; $CAM_PLAIN; cam_obstruct 1; camtrace 1; cubetrace 1" \
    --exec-at 20 "cube 3" \
    --exec-at 60 "teleport 7100 1024 15600" \
    --exec-at 100 "camnudge 30 0 80" \
    --tick 260 --exit > "$inbox" 2>&1 || fail "shot-box run failed: exit $?"

# "<shots fired> <frames in the box hidden and answered> <of those, frames with no extra at all>
# <frames recentred> <largest extra in the box>"
read -r box_shots box_answered box_bare box_recentred box_extra <<< "$(cam_tsv "$inbox" | awk '
    NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
    $col["zone"] == 1 && !inzone { shots++ }
    { inzone = $col["zone"] }
    $col["ext"] == 1 && $col["inshot"] == 1 {
        if ($col["hid"] == 2) { answered++; if ($col["obs"] == 0) bare++ }
        if ($col["hid"] == 3) recentred++
        if ($col["obs"] > extra) extra = $col["obs"]
    }
    END { print shots + 0, answered + 0, bare + 0, recentred + 0, extra + 0 }')"

[ "$box_shots" -ge 1 ] && [ "$box_answered" -ge 1 ] \
    || fail "the shot over the door fired $box_shots time(s) and the hero was hidden in its box on $box_answered frame(s): nothing to answer"
[ "$box_bare" -eq 0 ] && [ "$box_extra" -ge 100 ] \
    || fail "in the box of a fired shot the hero was hidden on $box_bare frame(s) with no extra pitch at all, and the extra only reached $box_extra"
[ "$box_recentred" -eq 0 ] \
    || fail "in the box of a fired shot the hero was recentred on $box_recentred frame(s)"

pass "out of the Tavern the pitch went from $off_own to $off_hi and stayed there with the setting off; with it on the pitch stayed $on_own, the extra was $on_first on arrival, 0 with the camera $on_leastpan units round, $on_last back behind him, at most $on_jump a frame, and he was never hidden; starting from $up_first with the hurry at 6 it reached $up_last with him hidden on $up_hurried frame(s), at most $up_jump a frame ($cut_jump with the hurry at 1), and no recentre; at the seam the pan $soff_before was recentred away with it off and kept with it on, over $son_hurried hidden frame(s); in the box of a fired shot he was hidden on $box_answered frame(s), each answered, the extra reaching $box_extra"
