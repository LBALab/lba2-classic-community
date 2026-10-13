# shellcheck shell=bash
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
    grep -q "\[INFO\] \[cam\]" "$log" || fail "no camera trace in the run (is this an exterior?)"
}

# cam_tsv <log> -- one row per traced frame, in frame order, with a header.
#
# A captured run holds every trace line twice: the log's own "[INFO] [cam] ..." and the console's
# echo of it, "[control] [cam] ...". They are two streams written to one capture, and they
# collide: the log's line regularly starts in the middle of an echo, glued behind whatever of the
# echo had been written so far. So a record is taken from wherever "[INFO] [cam]" appears, not
# only from the start of a line. Reading only the lines that began with it dropped the glued ones,
# four or five frames in a hundred and twenty, without a word. Every per-frame count came out
# short, and which frames went missing moved whenever the trace line changed length, so two builds
# that behaved identically printed different figures.
#
# The log's record is whole from its marker to the end of that line: it is written in one piece,
# and it is the echo that gets cut. The echo is ignored altogether, so nothing is counted twice.
# Should a record ever arrive cut, it cannot be rebuilt, and the count of those is reported on
# stderr instead of being left for someone to notice.
#
# Fields are read by name rather than position: each value is taken from the token after its
# label, so the order below is the column order and not an assumption about the line.
cam_tsv() {
    python3 - "$1" <<'PY'
import sys
# Every field the trace carries. A field the trace emits and this does not name is simply
# invisible to the fixtures: a query for it comes back empty and reads as "the thing never
# happened" rather than as an error. Keep this in step with the Log_Info call in FOLLOWCAM.CPP.
KEYS = ("heroBeta", "add", "beta", "target", "step", "delay", "orbit", "moving",
        "zone", "forced", "cine", "follow", "ext", "vue", "alpha", "dist", "inshot", "loan", "obs", "hid")
MARK = "[INFO] [cam]"
print("\t".join(KEYS))
cut = 0
for line in open(sys.argv[1], errors="replace"):
    at = line.find(MARK)
    if at < 0:
        continue
    toks = line[at + len(MARK):].split()
    vals = {}
    for i, tok in enumerate(toks):
        if tok in KEYS and tok not in vals and i + 1 < len(toks):
            vals[tok] = toks[i + 1]
    if len(vals) == len(KEYS):
        print("\t".join(vals[k] for k in KEYS))
    else:
        cut += 1
if cut:
    sys.stderr.write("cam_tsv: %d trace record(s) arrived cut and were left out\n" % cut)
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
