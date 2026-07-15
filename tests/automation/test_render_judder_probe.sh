#!/usr/bin/env bash
# Phase 0 characterization (docs/RENDER_INTERP_PLAN.md). The fixed-timestep sim (#358) re-presents a
# frozen pose on throttle-skip frames, so on a display faster than ~60 Hz held movement staircases.
# The `rendertrace` probe logs the hero's drawn position every RENDERED frame; at --fixed-dt 4 (240 Hz
# emulation, one sim step every 4th frame) ~3 of every 4 frames repeat the previous position exactly.
#
# This pins that staircase as an executable fact -- the RED baseline that render interpolation will
# flip. When Phase 2's RenderInterp lands, the same measurement drops toward zero (with the flag on),
# and this test grows a second assertion for that. The probe reads the sim position today; it reads the
# interpolated draw position once interpolation overwrites it before AffScene.
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips otherwise.
TESTNAME=render_judder_probe
. "$(dirname "$0")/lib.sh"
precheck

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/V Jump.LBA"
[ -f "$SAVE" ] || skip "no V Jump corpus save"

sv="$(mktemp --suffix=.LBA)"; home="$(mktemp -d)"; cp "$SAVE" "$sv"
# --fixed-dt 4 emulates 240 Hz (a sim step every 4th rendered frame); hold forward and trace the drawn
# hero position each rendered frame into an isolated debug log.
XDG_DATA_HOME="$home" ctl_headless --load "$sv" --fixed-dt 4 --log-level debug \
    --exec "rendertrace 1;input up 100000" --tick 400 --exit >/dev/null 2>&1
log="$home/Twinsen/LBA2/adeline.log"

result="$(python3 - "$log" <<'PY'
import sys, re, math
pts = []
for ln in open(sys.argv[1], errors='ignore'):
    m = re.search(r'\[rendertrace\] frame \d+ pos=\((-?\d+),(-?\d+),(-?\d+)\)', ln)
    if m:
        pts.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
w = pts[40:]  # skip the initial settle frames; measure the steady glide
if len(w) < 20:
    print("0 0 0"); sys.exit()
deltas = [math.dist(w[i], w[i - 1]) for i in range(1, len(w))]
moved = int(math.dist(w[-1], w[0]))
zero = sum(1 for d in deltas if d == 0)
print(f"{moved} {100 * zero // len(deltas)} {len(deltas)}")
PY
)"
rm -rf "$sv" "$home"

moved="$(echo "$result" | awk '{print $1}')"
zerofrac="$(echo "$result" | awk '{print $2}')"
total="$(echo "$result" | awk '{print $3}')"

[ -n "$total" ] && [ "$total" -gt 0 ] || fail "no rendertrace output (probe broken?)"
[ "$moved" -gt 500 ] || fail "hero barely moved ($moved); bad fixture"
# The staircase: with the sim stepping every 4th frame, a strong majority of moving frames repeat the
# hero position exactly. Assert that majority -- it is the judder, as an executable fact. Phase 2
# (RenderInterp on) drives this toward 0.
[ "$zerofrac" -ge 60 ] || fail "expected a high zero-delta fraction (staircase); got ${zerofrac}%"
pass "render staircase present: ${zerofrac}% of moving frames repeat the hero position (moved=$moved)"
