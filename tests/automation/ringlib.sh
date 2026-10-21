# shellcheck shell=bash
# Shared helpers for the ring contract fixtures (test_ring_*.sh).
#
# The rings Decision (docs/knowledge/decisions/a-camera-setting-is-watched-not-played.md)
# states two contracts for the Auto camera's settings, and these helpers are how the
# fixtures check them:
#
#   - With the Auto camera on, flipping any of its settings changes how the run is seen,
#     never what happens: the world hash of a replay matches (test_ring_watch_not_play.sh).
#   - With the Auto camera off, flipping any of them changes nothing at all: world hash,
#     view hash and the picture all match (test_ring_off_is_gone.sh).
#
# The method is the one the world/view split was measured with: record a scene once, then
# replay it with --replay-own-settings and one setting flipped by the console at the start,
# and read the first world and view mismatch the replay prints.
#
# Sourced after lib.sh.

RING_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"

# Every Auto camera setting, as the console names it, and the value a fixture flips it
# to. The value is chosen to differ from the default (bools are the opposite; numbers
# move to an end of their range or well away from the default).
#
# The list is checked against the binary's own `varlist cam_` (ring_check_rows), so a new
# camera setting fails both fixtures until it is added here. That is the point: a setting
# nobody has flipped is one nobody has shown to be watched, not played. cam_follow is
# left out because it is the switch the fixtures hold, not one they flip.
RING_FLIPS=(
    cam_ease_ms=0
    cam_glide=0
    cam_ground=0
    cam_ground_clear=2000
    cam_hd=0
    cam_hd_cap=70
    cam_hd_dist=60
    cam_hd_lean=0
    cam_hd_pitch=400
    cam_hold_angle=0
    cam_manual_hold=0
    cam_mouse=0
    cam_mouse_div=1
    cam_mouse_drag=0
    cam_mouse_invert=1
    cam_mouse_x=10
    cam_mouse_y=10
    cam_obstruct=0
    cam_obstruct_hurry=1
    cam_recenter_delay=0
    cam_recenter_drift=2
    cam_scene_carry=0
    cam_shot_blend_ms=0
    cam_smooth=1
    cam_stick_analog=0
    cam_stick_invert=1
    cam_stick_max_x=200
    cam_stick_max_y=100
    cam_stick_x=10
    cam_stick_y=10
    cam_target_ms=0
    cam_world_anchor=1
    cam_world_return=0
    cam_zone_anchor=1
    cam_zone_return=0
    cam_zone_skip=0
)

# ring_check_rows: every cam_ setting the binary has is in RING_FLIPS, and nothing else is.
ring_check_rows() {
    local have want missing extra
    have="$(ctl_headless --exec 'varlist cam_' --tick 2 --exit 2>/dev/null |
        sed -n 's/^\[control\] *\(cam_[a-z_]*\) - .*/\1/p' | grep -vx cam_follow | sort)"
    [ -n "$have" ] || fail "varlist cam_ listed no camera settings: cannot check the flip list"
    want="$(printf '%s\n' "${RING_FLIPS[@]}" | sed 's/=.*//' | sort)"
    missing="$(comm -23 <(printf '%s\n' "$have") <(printf '%s\n' "$want") | tr '\n' ' ')"
    extra="$(comm -13 <(printf '%s\n' "$have") <(printf '%s\n' "$want") | tr '\n' ' ')"
    [ -z "$missing" ] ||
        fail "camera settings with no flip in ringlib.sh RING_FLIPS: ${missing% }; add each with a value that differs from its default"
    [ -z "$extra" ] || fail "RING_FLIPS names settings the binary does not have: ${extra% }"
}

# The scenes, from the measurement behind the world/view split: ticks, the lba2.cfg lines
# the scene starts from on top of the Auto camera switch, and the engine arguments that
# drive it.
#
#   latch90   a shot that latches (cube 90), walked in and out twice, turning inside its box once
#   forced97  a shot that is forced (cube 97), walked through
#   orbit97   the camera turned by the console, a stick and a mouse (cube 97), sideways
#             and up and down, then a walk long enough for a recentre to start
#   hd97      orbit97 at 1920x1080, where the HD recompose acts
#   seam42    a walk over the seam into the next outdoor scene (cube 42 into 43)
#   door131   a door into an interior and out (cube 131)
#
# Two more start from a base that is not the default, because some settings only act
# under another one: latch90s with cam_zone_skip off, so the latching shot fires and the
# settings about a fired shot have one to act on; orbit97r with cam_hold_angle off and
# cam_world_anchor on, the recentring that cam_recenter_drift, cam_manual_hold and
# cam_world_return pace.
ring_scene() { # ring_scene <name>: sets SCENE_TICKS, SCENE_RES, SCENE_CFG and SCENE_ARGS
    SCENE_RES=640x480
    SCENE_CFG=""
    case "$1" in
    latch90 | latch90s)
        SCENE_TICKS=220
        SCENE_ARGS=(--exec "cube 90" --exec-at 30 "key up 170"
            --exec-at 40 "teleport 18000 3900 8000"
            --exec-at 70 "teleport 13000 4000 8000" --exec-at 78 "key left 12"
            --exec-at 100 "teleport 18000 3900 8000" --exec-at 130 "teleport 13000 4000 8000")
        [ "$1" = latch90s ] && SCENE_CFG="FollowCamZoneSkip: 0" ;;
    forced97)
        SCENE_TICKS=260
        SCENE_ARGS=(--exec-at 20 "teleport 15000 1000 21000" --exec-at 40 "key up 180") ;;
    orbit97 | hd97 | orbit97r)
        SCENE_TICKS=380
        SCENE_ARGS=(--exec-at 20 "key up 250" --exec-at 60 "key left 40"
            --exec-at 80 "camnudge 40 10 30" --exec-at 130 "stick 15000 -14000 60"
            --exec-at 200 "mouse -40 -40 30" --exec-at 232 "mouse 40 0 6 0"
            --exec-at 240 "key right 30" --exec-at 275 "key down 100")
        [ "$1" = hd97 ] && SCENE_RES=1920x1080
        [ "$1" = orbit97r ] && SCENE_CFG="FollowCamHoldAngle: 0
FollowCamWorldAnchor: 1" ;;
    seam42)
        SCENE_TICKS=520
        SCENE_ARGS=(--exec "cube 42" --exec-at 20 "teleport 5000 250 19200 3072"
            --exec-at 40 "key up 460") ;;
    door131)
        SCENE_TICKS=420
        SCENE_ARGS=(--exec "cube 131" --exec-at 30 "teleport 6400 5600 9472 0"
            --exec-at 150 "teleport 8448 1280 9472 1024" --exec-at 260 "key up 140") ;;
    *) fail "no ring scene named $1" ;;
    esac
}
RING_SCENES="latch90 latch90s forced97 orbit97 orbit97r hd97 seam42 door131"

# ring_engine <user-dir> <follow> <res> <cfg> <args...>: one engine run in a folder of its
# own, with the Auto camera switch and the scene's base written to its lba2.cfg.
#
# Each run gets a fresh folder because a console write persists to its lba2.cfg, and a
# bare bool name toggles: a shared folder would carry one run's flip into the next.
#
# The base is written to the file, not set on the console: a console write lands after
# the first tick, so a replay that set cam_follow the same way as its recording still
# differed in the view at tick 1, with nothing flipped.
ring_engine() {
    local ud="$1" follow="$2" res="$3" cfg="$4"
    shift 4
    mkdir -p "$ud"
    { printf 'FollowCamera: %s\n' "$follow"; [ -z "$cfg" ] || printf '%s\n' "$cfg"; } >"$ud/lba2.cfg"
    LBA2_USER_DIR="$ud" timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" --headless --no-autosave \
        ${NO_PACE[@]+"${NO_PACE[@]}"} --language English --resolution "$res" --vsync on --fixed-dt 16 "$@"
}

# ring_record <work> <scene> <follow>: record the scene with the Auto camera at <follow>.
ring_record() {
    local work="$1" scene="$2" follow="$3"
    ring_scene "$scene"
    ring_engine "$work/$scene.rec-ud" "$follow" "$SCENE_RES" "$SCENE_CFG" --load "$RING_SAVE" \
        "${SCENE_ARGS[@]}" \
        --record "$work/$scene.rec" --tick "$SCENE_TICKS" --exit >"$work/$scene.rec.out" 2>&1 ||
        fail "$scene: recording run exited non-zero ($?)"
    [ -s "$work/$scene.rec" ] || fail "$scene: the recording run wrote no file"
}

# ring_defaults <work> <scene> <follow>: what each camera setting holds at the start of the
# scene's runs, into RING_DEFAULTS as name=value lines.
#
# Each name is run bare twice. A number prints its value both times; a bool toggles and
# prints the opposite, then toggles back and prints its value. So the second line is the
# value either way, in a run of its own that nothing else reads.
ring_defaults() {
    local work="$1" scene="$2" follow="$3" flip line="" tick=1 n=0 execs=()
    ring_scene "$scene"
    # In chunks on ticks of their own: a console line is cut off at its buffer, about a
    # kilobyte, and only the last --exec of a run is kept.
    for flip in "${RING_FLIPS[@]}"; do
        line="$line${flip%%=*}; ${flip%%=*}; "
        n=$((n + 1))
        if [ $((n % 8)) = 0 ]; then execs+=(--exec-at "$tick" "$line"); line=""; tick=$((tick + 1)); fi
    done
    [ -z "$line" ] || execs+=(--exec-at "$tick" "$line")
    ring_engine "$work/$scene.defaults-ud" "$follow" "$SCENE_RES" "$SCENE_CFG" \
        "${execs[@]}" --tick $((tick + 2)) --exit >"$work/$scene.defaults.out" 2>&1 ||
        fail "$scene: the defaults probe exited non-zero ($?)"
    RING_DEFAULTS="$(sed -n 's/^\[control\] *\(cam_[a-z_]*\) = \(-*[0-9]*\)$/\1=\2/p' "$work/$scene.defaults.out" |
        awk -F= '{ n[$1]++; if (n[$1] == 2) print }')"
    printf '%s\n' "$RING_DEFAULTS" >"$work/$scene.defaults"
    [ "$(printf '%s\n' "$RING_DEFAULTS" | grep -c =)" = "${#RING_FLIPS[@]}" ] ||
        fail "$scene: the defaults probe read $(printf '%s\n' "$RING_DEFAULTS" | grep -c =) of ${#RING_FLIPS[@]} settings"
}

# ring_use_defaults <work> <scene>: make a scene's defaults current again, for reading its
# replays after the other scenes have been set up.
ring_use_defaults() {
    RING_DEFAULTS="$(cat "$1/$2.defaults")"
}

# ring_is_default <name> <value>: whether the value is what the setting already holds in
# this scene (from the last ring_defaults), so that setting it there flips nothing.
ring_is_default() {
    printf '%s\n' "$RING_DEFAULTS" | grep -qx "$1=$2"
}

# At most RING_JOBS engine runs at once, one per processor unless set. Each is one
# process doing one thing, so more than the processors only queues them.
if [ -z "${RING_JOBS:-}" ]; then
    RING_JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"
fi
RING_RUNNING=0

# ring_spawned: count a run just started in the background, and once RING_JOBS are running
# wait for one to finish. Waiting for any one (wait -n, bash 4.3) keeps every slot busy;
# an older bash waits for the whole batch, so a batch takes as long as its slowest run.
ring_spawned() {
    RING_RUNNING=$((RING_RUNNING + 1))
    [ "$RING_RUNNING" -ge "$RING_JOBS" ] || return 0
    if [ "${BASH_VERSINFO[0]}" -gt 4 ] || { [ "${BASH_VERSINFO[0]}" -eq 4 ] && [ "${BASH_VERSINFO[1]}" -ge 3 ]; }; then
        wait -n
        RING_RUNNING=$((RING_RUNNING - 1))
    else
        wait
        RING_RUNNING=0
    fi
}

# ring_launch <work> <scene> <follow> <tag> <console> [args...]: start one replay of the
# scene's recording in the background, the console line run first. ring_read reads it.
# The screenshot is hashed in the same job, so the hashing runs in parallel too.
ring_launch() {
    local work="$1" scene="$2" follow="$3" tag="$4" console="$5"
    shift 5
    ring_scene "$scene"
    (
        ring_engine "$work/$scene.$tag.ud" "$follow" "$SCENE_RES" "$SCENE_CFG" --replay-own-settings \
            --replay "$work/$scene.rec" --exec "camtrace 1${console:+; $console}" "$@" \
            --tick $((SCENE_TICKS + 60)) --screenshot "$work/$scene.$tag.png" --exit \
            >"$work/$scene.$tag.out" 2>&1
        echo $? >"$work/$scene.$tag.rc"
        [ -f "$work/$scene.$tag.png" ] &&
            python3 "$REPO/scripts/dev/png_hash.py" "$work/$scene.$tag.png" >"$work/$scene.$tag.fhash"
    ) &
    ring_spawned
}
ring_wait() { wait; RING_RUNNING=0; }

# ring_read <work> <scene> <tag>: report a finished replay through RW (first world
# mismatch), RV (first view mismatch), RT (ticks compared), RF (a hash of the last frame),
# RS (a hash of the view drawn on every step) and RN (steps the camera trace logged).
# Called directly, never as $( ): `fail` must end the fixture.
#
# RS exists because the view hash does not see everything the camera does. It hashes
# the camera globals the game holds; a shot blend and the obstruction tilt are applied
# to the view that is drawn and never written back to them. The camera trace logs that
# drawn view (sbeta, salpha, sdist, sx, sy, sz) on every step.
ring_read() {
    local work="$1" scene="$2" tag="$3" out wv rc
    out="$work/$scene.$tag.out"
    ring_scene "$scene"
    rc="$(cat "$work/$scene.$tag.rc" 2>/dev/null)"
    [ "$rc" = 0 ] || fail "$scene/$tag: replay exited non-zero (${rc:-never finished}): hang or crash"
    wv="$(sed -n 's/.*world\/view: compared on \([0-9]*\) ticks, first world mismatch \(-*[0-9]*\), first view mismatch \(-*[0-9]*\).*/\1 \2 \3/p' "$out" | head -1)"
    [ -n "$wv" ] || fail "$scene/$tag: the replay printed no world/view line; it cannot be said to have matched"
    read -r RT RW RV <<<"$wv"
    # The recording holds SCENE_TICKS + 1 ticks, tick 0 included, and a clean replay
    # compares all of them. Fewer means the reader lost the stream, which reads like a
    # match on the ticks it did compare.
    [ "$RT" -ge $((SCENE_TICKS + 1)) ] || fail "$scene/$tag: compared $RT ticks of the $((SCENE_TICKS + 1)) recorded"
    [ -f "$work/$scene.$tag.png" ] || fail "$scene/$tag: no screenshot"
    RF="$(cat "$work/$scene.$tag.fhash" 2>/dev/null)"
    [ -n "$RF" ] || fail "$scene/$tag: could not hash the screenshot"
    RS="$(sed -n 's/.*\[cam\] .* sbeta \(.*\)$/\1/p' "$work/$scene.$tag.ud/adeline.log" | cksum | cut -d' ' -f1)"
    RN="$(grep -c '\[cam\] ' "$work/$scene.$tag.ud/adeline.log")"
}

# ring_flip_took <work> <scene> <tag> <name> <value>: the console echoed the flip, so the
# setting holds the value. A flip the console refused would pass every check vacuously.
ring_flip_took() {
    grep -q "^\[control\] *$4 = $5\$" "$1/$2.$3.out" ||
        fail "$2/$3: the console never confirmed $4 = $5, so the flip may not have happened"
}
