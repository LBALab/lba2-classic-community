#!/usr/bin/env bash
# A scripted shot taking the view from the Auto camera, and giving it back, is a blend with cam_shot_blend_ms on, not a cut.
#
# A camera zone writes its shot into the camera on the frame the hero steps into its box: the
# look-at point, the distance, the yaw and the pitch all change at once. Leaving the box, the
# Auto camera takes the view back the same way. cam_zone_return eases the yaw and the pitch back
# from there, but the look-at point and the distance still jump.
#
# With cam_shot_blend_ms above 0 the frame shows a blend from the view on screen to the new one
# over that many milliseconds instead. Only the frame: the camera's own state, which everything
# else reads, is what it was without the setting.
#
# The route is the covered bridge in the corpus save's own scene (cube 97), whose shot holds the
# view for as long as the hero stands under it: walk in from the west, turn round, walk out. The
# same on the Classic re-release data and the US disc.
#
# Every run starts from CAM_PLAIN, so cam_zone_return is off and the yaw and pitch the shot left
# are kept. That leaves a jump in all four on the way in and in the look-at point on the way out
# for the blend to take away, which the run with the setting at 0 shows is there.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=camzone_blend
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

case "$(ctl_headless --exec 'varlist' --tick 2 --exit 2>/dev/null)" in
    *cam_shot_blend_ms*) ;;
    *) skip "binary has no cam_shot_blend_ms setting" ;;
esac

work="$(mktemp -d -t lba2-camzone-blend-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# bridge_run <name> <cvars> [extra engine args...]
#
# West of the bridge facing east, settle, walk onto it (the shot fires about tick 147), turn
# round under the shot, and walk back off (it lets go about tick 397). A settings folder per run,
# since the cfg written at exit carries every cvar into the next boot.
bridge_run() {
    local name="$1" cvars="$CAM_PLAIN; $2"
    # "as shipped" leaves every setting where the build puts it, apart from any named after it.
    case "$2" in "as shipped"*) cvars="camtrace 1${2#as shipped}" ;; esac
    shift 2
    LBA2_USER_DIR="$work/user-$name"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cube 97" \
        --exec-at 15 "camtrace 1; cam_follow 1; cam_hold_angle 1; $cvars" \
        --exec-at 20 "teleport 10000 2500 15600 1024" \
        --exec-at 100 "input up 60" \
        --exec-at 200 "input left 100" \
        --exec-at 320 "input up 120" \
        "$@" --tick 460 --exit > "$work/$name.log" 2>&1 || fail "run $name failed: exit $?"
    # The run's own log, not the capture: this counts steps, and a record the console's echo
    # cut in the capture would be a step missing.
    cam_tsv "$LBA2_USER_DIR/adeline.log" > "$work/$name.tsv"
}

# A python reading of a run's table, for the questions awk makes long. Prints what it is asked.
#   in|out     the row the shot took the view on, or the row it gave it back on
#   jump <from> <to> <field>   the largest one-row change in a shown field over those rows
#                              (sbeta the short way round, look the look-at point's distance)
#   differs <tsv>  the first row whose camera state differs from that run's, -1 if none
#   recentre       the first row recentred for a hidden hero, -1 if none
#   recentreblending  rows recentred for a hidden hero while a blend was drawn
#   settled <row>  1 if that row shows the camera's own view with no blend running
#   blendrows <from>           rows from there on with a blend running, until the first without
#   mismatch   rows on which the frame shows something other than the camera's own state
#   startsheld <row>           1 if that row shows what the row before it showed
read_run() {
    python3 - "$work/$1.tsv" "${@:2}" <<'PY'
import sys, math
rows = list(__import__("csv").DictReader(open(sys.argv[1]), delimiter="\t"))
ask = sys.argv[2:]
I = lambda r, k: int(r[k])
def shown(r): return (I(r, "sbeta"), I(r, "salpha"), I(r, "sdist"), I(r, "sx"), I(r, "sy"), I(r, "sz"))
def own(r): return (I(r, "beta"), I(r, "alpha") + I(r, "obs"), I(r, "dist"), I(r, "vx"), I(r, "vy"), I(r, "vz"))
def change(a, b, k):
    if k == "sbeta":
        d = (I(b, k) - I(a, k)) % 4096
        return abs(d - 4096 if d > 2048 else d)
    if k == "look":
        return int(math.dist([I(a, c) for c in ("sx", "sy", "sz")], [I(b, c) for c in ("sx", "sy", "sz")]))
    return abs(I(b, k) - I(a, k))
q = ask[0]
if q in ("in", "out"):
    want = ("0", "1") if q == "in" else ("1", "0")
    print(next((i for i in range(1, len(rows)) if (rows[i - 1]["zone"], rows[i]["zone"]) == want), -1))
elif q == "jump":
    # A row the hidden-hero recovery recentred on is a cut made on purpose, not a jump.
    lo, hi, k = int(ask[1]), int(ask[2]), ask[3]
    print(max([0] + [change(rows[i - 1], rows[i], k) for i in range(max(lo, 1), min(hi, len(rows))) if rows[i]["hid"] != "3"]))
elif q == "blendrows":
    i = n = int(ask[1])
    while i < len(rows) and I(rows[i], "blend") > 0:
        i += 1
    print(i - n)
elif q == "mismatch":
    print(sum(1 for r in rows if shown(r) != own(r)))
elif q == "startsheld":
    i = int(ask[1])
    print(1 if shown(rows[i]) == shown(rows[i - 1]) else 0)
elif q == "settled":
    i = int(ask[1])
    print(1 if shown(rows[i]) == own(rows[i]) and I(rows[i], "blend") == 0 else 0)
elif q == "differs":
    # The first row on which another run's camera state is not this one's, -1 if none. What
    # the frame showed is left out, and so is `hid`, which reports what the frame drew.
    other = list(__import__("csv").DictReader(open(ask[1]), delimiter="\t"))
    skip = {"blend", "sbeta", "salpha", "sdist", "sx", "sy", "sz", "hid"}
    keys = [k for k in rows[0] if k not in skip]
    n = min(len(rows), len(other))
    bad = next((i for i in range(n) if any(rows[i][k] != other[i][k] for k in keys)), -1)
    print(bad if bad >= 0 or len(rows) == len(other) else n)
elif q == "recentre":
    # The first row the hidden-hero recovery recentred on, -1 if none.
    print(next((i for i, r in enumerate(rows) if r["hid"] == "3"), -1))
elif q == "recentreblending":
    # Rows that recentred for a hidden hero while a blend was being drawn.
    print(sum(1 for i in range(1, len(rows)) if rows[i]["hid"] == "3" and I(rows[i - 1], "blend") > 0))
PY
}

# --- The cut, with the setting at 0 -----------------------------------------------------------
bridge_run cut "cam_shot_blend_ms 0"

in_cut="$(read_run cut in)"
out_cut="$(read_run cut out)"
[ "$in_cut" -gt 0 ] && [ "$out_cut" -gt "$in_cut" ] \
    || fail "the shot never took the view and gave it back (in $in_cut, out $out_cut): the route missed the bridge"

# What the blend is there to take away has to be there: on the way in the yaw turns about 45
# degrees in a frame, on the way out the look-at point moves about two thousand units.
yaw_in_cut="$(read_run cut jump "$in_cut" "$((in_cut + 1))" sbeta)"
look_out_cut="$(read_run cut jump "$out_cut" "$((out_cut + 1))" look)"
[ "$yaw_in_cut" -ge 300 ] \
    || fail "with the setting at 0 the yaw turned only $yaw_in_cut in the frame the shot fired: no cut to blend"
[ "$look_out_cut" -ge 1000 ] \
    || fail "with the setting at 0 the look-at point moved only $look_out_cut as the shot let go: no cut to blend"

# At 0 the frame is the camera's own state on every row.
[ "$(read_run cut mismatch)" = 0 ] \
    || fail "with the setting at 0 the frame showed something other than the camera on $(read_run cut mismatch) rows"

# --- The blend ----------------------------------------------------------------------------------
bridge_run blend "cam_shot_blend_ms 600"

in_b="$(read_run blend in)"
out_b="$(read_run blend out)"
# Only the frame changes, with one exception. The camera's own state, and so the hero, the shot
# and everything else that reads it, is the run with the setting at 0 on every row up to the
# first time that run recentres for a hidden hero. That recovery looks at what was drawn, and
# here, with cam_obstruct off, the shot's own view hides the hero as he walks out, a few steps
# into the blend. A view on its way between two others is no reason to put the camera anywhere,
# so the recovery waits for the blend to end and is made on the view it ends on.
differs="$(read_run blend differs "$work/cut.tsv")"
recentre_cut="$(read_run cut recentre)"
[ "$differs" = -1 ] || { [ "$recentre_cut" -ge 0 ] && [ "$differs" -ge "$recentre_cut" ]; } \
    || fail "the blend changed the camera's own state from row $differs on, before any recentre (first at $recentre_cut): it reached past the frame"
[ "$(read_run blend recentreblending)" = 0 ] \
    || fail "the hidden-hero recovery recentred on a view the blend was drawing"
[ "$in_b" = "$in_cut" ] && [ "$out_b" = "$out_cut" ] \
    || fail "the blend moved the shot (in $in_b out $out_b against $in_cut $out_cut)"

for edge in in out; do
    at="$in_b"; [ "$edge" = out ] && at="$out_b"
    [ "$(read_run blend startsheld "$at")" = 1 ] \
        || fail "on the way $edge the first frame did not show what the one before it did: the blend started off the screen"
    n="$(read_run blend blendrows "$at")"
    # 600 ms of 16 ms steps.
    [ "$n" -ge 36 ] && [ "$n" -le 39 ] \
        || fail "on the way $edge the blend ran for $n steps, not about 37 (600 ms)"
    [ "$(read_run blend settled "$((at + n))")" = 1 ] \
        || fail "on the way $edge the blend did not end on the camera's own view"
    yaw="$(read_run blend jump "$at" "$((at + n + 1))" sbeta)"
    pitch="$(read_run blend jump "$at" "$((at + n + 1))" salpha)"
    dist="$(read_run blend jump "$at" "$((at + n + 1))" sdist)"
    look="$(read_run blend jump "$at" "$((at + n + 1))" look)"
    [ "$yaw" -le 60 ] && [ "$pitch" -le 20 ] && [ "$dist" -le 60 ] && [ "$look" -le 300 ] \
        || fail "on the way $edge the frame still jumped: yaw $yaw, pitch $pitch, distance $dist, look-at $look in one step"
done

# --- Center camera in the middle of the blend out is still a cut -------------------------------
bridge_run center "cam_shot_blend_ms 600" --exec-at 410 "key enter 2"

out_c="$(read_run center out)"
[ "$out_c" = "$out_cut" ] || fail "the shot let go on row $out_c, not $out_cut"
pressed="$(read_run center blendrows "$out_c")"
[ "$pressed" -lt 30 ] \
    || fail "the blend ran $pressed steps with Center camera pressed inside it: the press never landed"
[ "$(read_run center settled "$((out_c + pressed))")" = 1 ] \
    || fail "Center camera did not put the frame on the camera's own view at once"
[ "$(read_run center jump "$((out_c + pressed))" "$((out_c + pressed + 1))" sbeta)" -ge 300 ] \
    || fail "Center camera turned the frame by less than a cut would: the blend carried on across it"

# --- As shipped ----------------------------------------------------------------------------------
# With cam_obstruct on, as it ships, the hero is not hidden on this route, and the blend leaves
# the camera's own state exactly as the cut does on every row.
bridge_run shipped "as shipped"
bridge_run shipped_cut "as shipped; cam_shot_blend_ms 0"

in_s="$(read_run shipped in)"
[ "$in_s" -gt 0 ] || fail "as shipped, the shot never fired"
[ "$(read_run shipped blendrows "$in_s")" -ge 30 ] \
    || fail "as shipped, there is no blend into the shot"
[ "$(read_run shipped differs "$work/shipped_cut.tsv")" = -1 ] \
    || fail "as shipped, the blend changed the camera's own state from row $(read_run shipped differs "$work/shipped_cut.tsv") on"

pass "the shot's ${yaw_in_cut}-unit turn in and ${look_out_cut}-unit look-at jump out are blended over 600 ms, the camera's own state is untouched as shipped, and Center camera still cuts"
