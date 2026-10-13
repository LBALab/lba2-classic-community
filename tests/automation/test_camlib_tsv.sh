#!/usr/bin/env bash
# camlib's trace reader returns every frame the log recorded, including the ones glued behind a console echo.
#
# A captured run holds each camera trace line twice, once from the log and once echoed by the
# console, and the two streams collide: the log's line often starts in the middle of an echo.
# cam_tsv used to read only the lines that began with the log's marker, which dropped the glued
# ones silently. Per-frame counts came out a few percent short, which frames went missing
# depended on the length of the trace line, and a fixture watching for an event that lasts one
# frame could miss it.
#
# This needs no engine and no game data: it feeds the reader a capture written out by hand, in
# the shapes the collisions take, and checks what comes back.
TESTNAME=camlib_tsv
. "$(dirname "$0")/lib.sh"
. "$(dirname "$0")/camlib.sh"

work="$(mktemp -d -t lba2-camlib-tsv-XXXXXX)"
trap 'rm -rf "$work"' EXIT

# One whole trace record, with the frame's BetaCam as the only thing that varies.
rec() { # <beta> [zone]
    printf 'heroBeta  174 add    0 beta %4d target 1874 step    17 delay   0 orbit 0 moving 0 zone %d forced 1 cine 0 follow 1 ext 1 vue 0 alpha  342 dist  13750' \
        "$1" "${2:-0}"
}

{
    # A clean pair: the log's line, then the console's echo of it.
    printf '[INFO] [cam] %s\n' "$(rec 1001)"
    printf '[control] [cam] %s\n' "$(rec 1001)"
    # The log's line glued behind the first bytes of an echo.
    printf '[control][INFO] [cam] %s\n' "$(rec 1002)"
    printf ' [cam] %s\n' "$(rec 1002)"
    # The log's line landing in the middle of an echo, whose tail then starts the next line.
    printf '[control] [cam] heroBeta  174 add    0 beta 1003 target 1874 step    17 delay   0 orbit 0 moving 0 zo[INFO] [cam] %s\n' "$(rec 1003 1)"
    printf 'ne 1 forced 1 cine 0 follow 1 ext 1 vue 0 alpha  342 dist  13750\n'
    # A record from a build whose trace carries fields this reader does not name yet.
    printf '[INFO] [cam] %s inshot 0 loan 0\n' "$(rec 1004)"
    # A record that arrived cut: it cannot be rebuilt, and has to be reported, not hidden.
    printf '[INFO] [cam] heroBeta  174 add    0 beta 1005 target\n'
    # Unrelated output that mentions neither.
    printf '[INFO] Joystick   not detected\n'
} > "$work/capture.log"

cam_tsv "$work/capture.log" > "$work/out.tsv" 2> "$work/err.txt"

rows="$(($(wc -l < "$work/out.tsv") - 1))"
[ "$rows" = 4 ] \
    || fail "the capture holds 4 whole trace records and the reader returned $rows"

betas="$(cam_col "$work/capture.log" beta 2>/dev/null | tr '\n' ' ')"
[ "$betas" = "1001 1002 1003 1004 " ] \
    || fail "the frames came back as '$betas', not 1001 1002 1003 1004 in order: an echo was counted, or a frame lost"

# The frame that fell in the middle of an echo is the one with zone set. An event that lasts a
# single frame is exactly what the old reader could lose.
zones="$(cam_col "$work/capture.log" zone 2>/dev/null | tr '\n' ' ')"
[ "$zones" = "0 0 1 0 " ] \
    || fail "the one-frame zone event read as '$zones', not on the third frame"

grep -q "1 trace record" "$work/err.txt" \
    || fail "the cut record was dropped without a word: stderr said '$(cat "$work/err.txt")'"

pass "4 of 4 whole records read in order, the one-frame event among them, and the cut record reported"
