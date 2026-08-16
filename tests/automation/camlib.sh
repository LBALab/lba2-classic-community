# Shared helpers for the follow-camera fixtures.
#
# The Auto camera is analog, so what needs asserting is rarely a single end state: it is the
# shape of a motion over frames. Whether the camera holds an angle while the hero turns under
# it, whether it tracks the stick at the speed asked for, whether it stops smoothly or halts.
# These helpers turn a `camtrace` run into a per-frame table so a fixture can assert on that
# shape in a couple of lines.
#
# Needs a binary with the camera console commands (`camtrace`, `camnudge`); cam_precheck skips
# cleanly without one.

# The corpus save the camera fixtures run in: island 6 cube 97, an exterior, which is where the
# Auto camera lives (interiors and camera zones run other code).
CAM_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"

# cam_precheck -- everything the camera fixtures need, on top of the usual precheck.
cam_precheck() {
    precheck
    [ -f "$CAM_SAVE" ] || skip "fixture missing: $CAM_SAVE"
    case "$(ctl_headless --exec 'help camtrace' --tick 2 --exit 2>/dev/null)" in
        *"Unknown help topic"*)
            skip "binary predates the camera console commands (camtrace/camnudge)" ;;
    esac
}

# cam_run <log> <first-tick-cmds> [extra engine args...]
#
# Boots the corpus save with the Auto camera on, hold-angle on and tracing on, appends the
# caller's own first-tick commands, and writes the run's output to <log>. Extra arguments
# (--exec-at timelines, --tick, cvar overrides) pass straight through.
cam_run() {
    local log="$1" execs="$2"
    shift 2
    ctl_headless --load "$CAM_SAVE" --fixed-dt 16 \
        --exec "cam_follow 1; cam_hold_angle 1; camtrace 1; $execs" \
        "$@" --exit > "$log" 2>&1 || fail "engine run failed: exit $?"
    grep -q "^\[INFO\] \[cam\]" "$log" || fail "no camera trace in the run (is this an exterior?)"
}

# cam_tsv <log> -- one row per follow-cam update, in frame order, with a header.
#
# Fields are read by name rather than position: the console echo and the log fan-out are two
# streams that interleave in a captured run, and their fields sit at different offsets, so only
# the log stream is read and each value is taken from the token after its label.
cam_tsv() {
    python3 - "$1" <<'PY'
import sys
KEYS = ("heroBeta", "add", "beta", "target", "step", "delay", "orbit", "moving")
print("\t".join(KEYS))
for line in open(sys.argv[1], errors="replace"):
    if not line.startswith("[INFO] [cam]"):
        continue
    toks = line.split()
    vals = {}
    for i, tok in enumerate(toks):
        if tok in KEYS and i + 1 < len(toks):
            vals[tok] = toks[i + 1]
    if len(vals) == len(KEYS):
        print("\t".join(vals[k] for k in KEYS))
PY
}

# cam_col <log> <field> -- that column's values, one per line, in frame order.
cam_col() {
    cam_tsv "$1" | awk -v want="$2" '
        NR == 1 { for (i = 1; i <= NF; i++) if ($i == want) col = i; next }
        { print $col }'
}

# cam_lag <log> -- per frame, how far the camera is from the angle it is heading for
# (target - beta), taken the shortest way round the 4096-unit circle and unsigned.
cam_lag() {
    cam_tsv "$1" | awk '
        NR == 1 { next }
        { d = $4 - $3; if (d > 2048) d -= 4096; if (d < -2048) d += 4096; print (d < 0) ? -d : d }'
}
