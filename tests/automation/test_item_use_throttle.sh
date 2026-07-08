#!/usr/bin/env bash
# #392 regression: the fixed-timestep throttle must not drop a one-frame inventory item-use.
#
# A quest item-use is a one-frame signal (InventoryAction, consumed by LF_USE_INVENTORY inside
# DoLife). The throttle skips the whole simulation on frames that render faster than one step, so a
# use landing on a skipped frame would be lost before any simulated frame reads it. #392
# (Timer_ForceStepIfPending) forces a would-skip frame to simulate when a use is pending, so the
# NPC's poll still sees it.
#
# Driven end-to-end on a tracked save where NPC obj 2 polls LF_USE_INVENTORY for item 18 (the hero
# holds it). Throttle on at 100 ms with a fast clock (--fixed-dt 8) makes frames 2..13 all skip; the
# use is injected on frame 6 (delay 5) -- a guaranteed skip frame. With #392 that frame is forced
# and the poll returns TRUE; without it the use is dropped and the poll never returns TRUE. The give
# then triggers a blocking dialog (no clean --exit), so the run is SIGKILLed and the flushed
# adeline.log is read (Log_Info fflushes per line; stdout is block-buffered and lost on the kill).
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=item_use_throttle
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

# Isolate the log + cfg in a temp XDG dir: no stale runs, no touching the real adeline.log.
xdg="$(mktemp -d)"
trap 'rm -rf "$xdg"' EXIT
log="$xdg/Twinsen/LBA2/adeline.log"

# The give-dialog blocks the tick loop, so the run never exits cleanly: bound it and SIGKILL.
XDG_DATA_HOME="$xdg" SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
    timeout -s KILL 20 "$LBA2_BIN" \
    --language English --no-audio --resolution 640x480 --load "$FIX" \
    --fixed-timestep 100 --fixed-dt 8 --exec "useitem trace 1; useitem 18 1 5" --tick 20 --exit \
    >/dev/null 2>&1

[ -f "$log" ] || fail "no log written ($log) -- run did not boot?"
grep -q "num=18 .*InvAct=18.* -> TRUE" "$log" \
    || fail "item-use dropped on a throttle-skip frame -- #392 regression (log: $log)"

pass "item-use survived the throttle (registered on a forced skip frame)"
