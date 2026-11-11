#!/usr/bin/env bash
# A scripted shot lent to the Auto camera keeps its eye off the ground with cam_zone_height on.
#
# A camera zone that fires once lends the Auto camera its yaw and pitch, not its look-at point or
# its distance: the Auto camera swings its own arm about the hero's feet at the shot's pitch. Two
# things then put the eye in the ground, and the setting answers each:
#
# - A flat shot written about a raised point. The demo's Desert Island beach (scene 203) is 2.4
#   degrees about a point 1,100 over the sand: lent, the eye sits about 600 over the hero's feet,
#   and the terrain at the bottom of the frame is not drawn. With the setting on, the look-at
#   rises until the eye is 1,500 over the ground, never higher than the shot itself aimed.
# - A hill behind the hero. The lent yaw turns with him, so turning on the spot swings the arm
#   round. In the Emerald Moon shot of cube 75 a left turn takes the eye under the hill behind;
#   with the setting on the pitch rises as far as it takes instead.
#
# And a shot aimed at something far off and high is left alone: the demo's scene 209 aims at the
# harbour 5,800 beyond its box, and the frame there is the same with the setting on or off.
#
# The trace has no ground height, so the beach is read as the eye's height over the hero (standing
# on the sand) and the Moon as the extra pitch the setting draws. Only the frame changes: the
# camera's own state is the same with the setting on or off, on every row.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
# Tier: big
TESTNAME=camzone_height
# The reel runs 24,000 steps, past the default 90 s when -j 4 shares the machine.
: "${LBA2_TEST_TIMEOUT:=240}"
export LBA2_TEST_TIMEOUT
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"
cam_precheck

case "$(ctl_headless --exec 'varlist' --tick 2 --exit 2>/dev/null)" in
    *cam_zone_height*) ;;
    *) skip "binary has no cam_zone_height setting" ;;
esac

MOON_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Emerald Moon.LBA"
[ -f "$MOON_SAVE" ] || skip "fixture missing: $MOON_SAVE"

work="$(mktemp -d -t lba2-camzone-height-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# run_in <name>: a settings folder of its own, since the cfg written at exit carries every cvar
# into the next boot.
run_in() {
    LBA2_USER_DIR="$work/user-$1"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR"
}

# moon_run <name> <0|1>: a step south of cube 75's shot facing it, step in (the shot fires),
# then turn left on the spot. The shot takes the camera while the hero steers only with
# cam_zone_skip off, and lends it only with cam_zone_return on.
moon_run() {
    run_in "$1"
    ctl_headless --load "$MOON_SAVE" --fixed-dt 16 \
        --exec "cube 75" \
        --exec-at 15 "camtrace 1; cubetrace 1; cam_follow 1; $CAM_PLAIN; cam_zone_return 1; cam_zone_skip 0; cam_zone_height $2" \
        --exec-at 20 "teleport 30976 325 11700 0" \
        --exec-at 60 "input up 40" \
        --exec-at 160 "input left 600" \
        --tick 760 --exit > "$work/$1.out" 2>&1 || fail "run $1 failed: exit $?"
}

# reel_run <name> <0|1>: the demo reel as it ships, through the beach (203) and the harbour (209).
reel_run() {
    run_in "$1"
    ctl_headless --fixed-dt 16 --demo \
        --exec "camtrace 1; cubetrace 1; cam_follow 1; cam_zone_height $2; cube 193" \
        --tick 24000 --exit > "$work/$1.out" 2>&1 || fail "run $1 failed: exit $?"
}

# read_run <name> <question> [args]: reads the run's own log (not the capture, where the
# console's echo can cut a record), one row per [cam] record, each with the scene it is in.
#   segment <cube>      the first and one past the last row of that scene's first visit, or -1 -1
#   fired <lo> <hi> [maxpitch]  the first row in that range a shot took the view on, no steeper
#                       than maxpitch if given, -1 if none
#   eyeover <lo> <hi>   the lowest and highest eye height over the look-at point shown in that range
#   pitchplus           the largest pitch the frame drew over the camera's own, and on how many rows
#   mismatch            rows on which the frame shows something other than the camera's own view
#   differs <name>      the first row whose camera state differs from that run's, -1 if none
#   samefrom <name> <lo> <hi>  rows in that range whose frame differs from that run's
read_run() {
    python3 - "$work/user-$1/adeline.log" "$work" "${@:2}" <<'PY'
import sys, math, re
def load(path):
    rows, cube = [], -1
    for line in open(path, errors="replace"):
        if "[cube]" in line:
            m = re.search(r"-> (\d+)", line)
            if m: cube = int(m.group(1))
        elif "[INFO] [cam]" in line:
            t = line.split("[cam]", 1)[1].split()
            r = {t[i]: t[i + 1] for i in range(0, len(t) - 1, 2)}
            r["cube"] = cube
            rows.append(r)
    return rows
rows = load(sys.argv[1]); work = sys.argv[2]; ask = sys.argv[3:]
I = lambda r, k: int(r[k])
SHOWN = ("sbeta", "salpha", "sdist", "sx", "sy", "sz")
def shown(r): return tuple(I(r, k) for k in SHOWN)
def own(r): return (I(r, "beta"), I(r, "alpha") + I(r, "obs"), I(r, "dist"), I(r, "vx"), I(r, "vy"), I(r, "vz"))
def eyeover(r): return I(r, "sy") + int(I(r, "sdist") * math.sin(I(r, "salpha") * 2 * math.pi / 4096)) - I(r, "vy")
q = ask[0]
if q == "segment":
    c = int(ask[1]); lo = next((i for i, r in enumerate(rows) if r["cube"] == c), -1)
    hi = -1 if lo < 0 else next((i for i in range(lo, len(rows)) if rows[i]["cube"] != c), len(rows))
    print(lo, hi)
elif q == "fired":
    lo, hi = int(ask[1]), int(ask[2]); top = int(ask[3]) if len(ask) > 3 else 4096
    # From the scene's second row: its first follows another scene's, which is a cut, not a shot.
    print(next((i for i in range(lo + 1, hi) if rows[i - 1]["zone"] == "0" and rows[i]["zone"] == "1" and I(rows[i], "alpha") <= top), -1))
elif q == "eyeover":
    lo, hi = int(ask[1]), int(ask[2]); e = [eyeover(r) for r in rows[lo:hi]]
    print(min(e), max(e))
elif q == "pitchplus":
    p = [I(r, "salpha") - I(r, "alpha") - I(r, "obs") for r in rows if I(r, "blend") == 0]
    print(max(p), sum(1 for x in p if x > 0))
elif q == "mismatch":
    print(sum(1 for r in rows if I(r, "blend") == 0 and shown(r) != own(r)))
elif q == "differs":
    # What the frame showed is left out, and so is `hid`, which reports what the frame drew.
    other = load(f"{work}/user-{ask[1]}/adeline.log")
    skip = set(SHOWN) | {"blend", "hid", "cube"}
    keys = [k for k in rows[0] if k not in skip]
    n = min(len(rows), len(other))
    bad = next((i for i in range(n) if any(rows[i][k] != other[i].get(k) for k in keys)), -1)
    print(bad if bad >= 0 or len(rows) == len(other) else n)
elif q == "samefrom":
    other = load(f"{work}/user-{ask[1]}/adeline.log"); lo, hi = int(ask[2]), int(ask[3])
    print(sum(1 for i in range(lo, hi) if shown(rows[i]) != shown(other[i])))
PY
}

# --- The hill behind: Emerald Moon, cube 75 -------------------------------------------------------
moon_run moon_off 0
moon_run moon_on 1

[ "$(read_run moon_off fired 0 200)" -gt 0 ] || fail "on the Moon the shot never fired: the route missed its box"
[ "$(read_run moon_off mismatch)" = 0 ] \
    || fail "with the setting off the frame showed something other than the camera on $(read_run moon_off mismatch) rows"
read -r plus plusrows <<< "$(read_run moon_on pitchplus)"
[ "$plus" -ge 30 ] && [ "$plusrows" -ge 5 ] \
    || fail "turning under the Moon shot, the setting raised the pitch by at most $plus on $plusrows rows: the hill behind went unanswered"
[ "$(read_run moon_on differs moon_off)" = -1 ] \
    || fail "on the Moon the setting changed the camera's own state from row $(read_run moon_on differs moon_off) on: it reached past the frame"

# --- The flat shot: the demo's beach, and the harbour left alone -----------------------------------
reel_run reel_off 0
reel_run reel_on 1

read -r b_lo b_hi <<< "$(read_run reel_off segment 203)"
read -r h_lo h_hi <<< "$(read_run reel_off segment 209)"
[ "$b_lo" -ge 0 ] && [ "$h_lo" -ge 0 ] || fail "the reel never reached the beach (203) or the harbour (209)"
[ "$(read_run reel_on segment 203)" = "$b_lo $b_hi" ] && [ "$(read_run reel_on segment 209)" = "$h_lo $h_hi" ] \
    || fail "the setting moved the reel's scenes"
# The beach's flat shot, 27 units (2.4 degrees) of pitch; the scene has steeper ones before it.
fired="$(read_run reel_off fired "$b_lo" "$b_hi" 60)"
[ "$fired" -gt 0 ] || fail "on the beach the flat shot never fired"
# Once the blend into the shot is over, while the hero is still in its box.
read -r off_lo off_hi <<< "$(read_run reel_off eyeover "$((fired + 60))" "$((fired + 120))")"
read -r on_lo on_hi <<< "$(read_run reel_on eyeover "$((fired + 60))" "$((fired + 120))")"
[ "$off_hi" -lt 1000 ] \
    || fail "with the setting off the beach's eye was already $off_lo..$off_hi over the hero: no low eye to raise"
[ "$on_lo" -ge 1300 ] \
    || fail "with the setting on the beach's eye was only $on_lo..$on_hi over the hero (off: $off_lo..$off_hi)"
[ "$(read_run reel_on samefrom reel_off "$h_lo" "$h_hi")" = 0 ] \
    || fail "the harbour's frame changed on $(read_run reel_on samefrom reel_off "$h_lo" "$h_hi") rows: a shot aimed far off lifted the camera"
[ "$(read_run reel_on differs reel_off)" = -1 ] \
    || fail "in the reel the setting changed the camera's own state from row $(read_run reel_on differs reel_off) on"

pass "the Moon's hill is answered with up to $plus of pitch, the beach's eye rises from $off_lo..$off_hi to $on_lo..$on_hi over the hero, the harbour is untouched, and only the frame changes"
