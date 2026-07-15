#!/usr/bin/env bash
# Render interpolation, Phases 0 + 2 (docs/RENDER_INTERP_PLAN.md). The fixed-timestep sim (#358)
# re-presents a frozen pose on throttle-skip frames, so held movement staircases above ~60 Hz. The
# `rendertrace` probe logs the hero's drawn position every rendered frame; at --fixed-dt 4 (240 Hz
# emulation) ~3 of every 4 frames repeat it. Render interpolation (RenderInterp, #412) draws the hero
# between its last two sim-step positions so nearly every frame advances -- and it is render-only, so
# the SIM position (what --dump-state reports) is byte-identical with the flag on or off.
#
# Asserts: (baseline) the staircase is present with interp off; (fix) interp on drops the zero-delta
# fraction well below it; (safety) the end-of-run sim position is identical on vs off at both
# --fixed-dt 4 (interp active -> Apply/Restore leaves the sim byte-exact) and --fixed-dt 16 (no skip
# frames -> a pure no-op). A restore bug would diverge the sim dumps.
#
# Local-only: needs retail data + a build + the tracked steam_classic_2023 corpus; skips otherwise.
TESTNAME=render_judder_probe
. "$(dirname "$0")/lib.sh"
precheck

SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/V Jump.LBA"
[ -f "$SAVE" ] || skip "no V Jump corpus save"

# "zerofrac moved frames" for a forward glide at --fixed-dt 4 with the given exec prefix.
zerofrac() {
    local sv home log; sv="$(mktemp --suffix=.LBA)"; home="$(mktemp -d)"; cp "$SAVE" "$sv"
    XDG_DATA_HOME="$home" ctl_headless --load "$sv" --fixed-dt 4 --log-level debug \
        --exec "${1}rendertrace 1;input up 100000" --tick 400 --exit >/dev/null 2>&1
    log="$home/Twinsen/LBA2/adeline.log"
    python3 - "$log" <<'PY'
import sys, re, math
pts = [tuple(int(g) for g in m.groups())
       for ln in open(sys.argv[1], errors='ignore')
       for m in [re.search(r'\[rendertrace\] frame \d+ pos=\((-?\d+),(-?\d+),(-?\d+)\)', ln)] if m]
w = pts[40:]
if len(w) < 20:
    print("0 0 0"); sys.exit()
d = [math.dist(w[i], w[i - 1]) for i in range(1, len(w))]
z = sum(1 for x in d if x == 0)
print(f"{100 * z // len(d)} {int(math.dist(w[-1], w[0]))} {len(d)}")
PY
    rm -rf "$sv" "$home"
}

# End-of-run hero SIM position (--dump-state reports the true sim position, not the drawn one).
sim_pos() { # fixedtimestep-ms exec-prefix
    local sv d; sv="$(mktemp --suffix=.LBA)"; d="$(mktemp)"; cp "$SAVE" "$sv"
    ctl_headless --load "$sv" --fixed-dt "$1" --exec "${2}input up 100000" --tick 300 --dump-state "$d" --exit >/dev/null 2>&1
    python3 -c "import json,sys;h=json.load(open(sys.argv[1]))['hero'];print(h['x'],h['y'],h['z'])" "$d" 2>/dev/null
    rm -f "$sv" "$d"
}

off="$(zerofrac "")"
on="$(zerofrac "renderinterp 1;")"
off_zf="$(echo "$off" | awk '{print $1}')"; off_moved="$(echo "$off" | awk '{print $2}')"; off_n="$(echo "$off" | awk '{print $3}')"
on_zf="$(echo "$on" | awk '{print $1}')"; on_n="$(echo "$on" | awk '{print $3}')"

[ -n "$off_n" ] && [ "$off_n" -gt 0 ] && [ -n "$on_n" ] && [ "$on_n" -gt 0 ] || fail "no rendertrace output (probe broken?)"
[ "$off_moved" -gt 500 ] || fail "hero barely moved ($off_moved); bad fixture"

# Baseline: the staircase is present with interpolation off (a strong majority of frames repeat).
[ "$off_zf" -ge 60 ] || fail "expected the staircase with interp off; got ${off_zf}% zero-delta"
# Fix: interpolation drops the zero-delta fraction well below the staircase (measured ~11% vs ~77%).
[ "$on_zf" -lt 30 ] || fail "interpolation did not smooth motion: ${on_zf}% zero-delta (off is ${off_zf}%)"

# Safety: render-only, so the SIM position is byte-identical on vs off, both where interp is active
# (--fixed-dt 4) and where it is a no-op (--fixed-dt 16). A restore bug diverges these.
for fdt in 4 16; do
    a="$(sim_pos "$fdt" "")"; b="$(sim_pos "$fdt" "renderinterp 1;")"
    [ -n "$a" ] && [ "$a" = "$b" ] || fail "sim diverged at --fixed-dt $fdt: off='$a' on='$b' (restore bug)"
done

pass "interp smooths the glide (zero-delta ${off_zf}% -> ${on_zf}%) and the sim stays byte-exact on/off"
