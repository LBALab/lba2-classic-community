#!/usr/bin/env bash
# `skipmodals` console command: a headless run gets past a give/talk dialog instead of hanging.
#
# When an NPC accepts an item it shows a dialogue, which spins waiting for a keypress while the main
# tick loop is stalled -- so a headless give hangs (this is why test_item_use_throttle.sh has to
# SIGKILL and scrape the log). With `skipmodals 1` the dialog aborts immediately and the script
# continues, so the run completes AND the give still registers.
#
# On a tracked save where NPC obj 2 accepts item 18, inject the use with skipmodals on, and assert:
# the run exits cleanly (--exit reached, no hang) and the item-use registered (LF_USE_INVENTORY 18
# -> TRUE in the flushed log). Isolated XDG dir so we read only this run's log.
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=skip_modals
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

xdg="$(mktemp -d)"
trap 'rm -rf "$xdg"' EXIT
log="$xdg/Twinsen/LBA2/adeline.log"

# With skipmodals the run must EXIT cleanly (no dialog hang). Bound it so a regression (hang) fails
# fast instead of stalling the suite; a clean run finishes in a couple of seconds.
XDG_DATA_HOME="$xdg" SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
    timeout -s KILL 30 "$LBA2_BIN" \
    --language English --no-audio --resolution 640x480 --load "$FIX" --fixed-dt 16 \
    --exec "skipmodals 1; useitem trace 1; useitem 18 1 3" --tick 40 --exit \
    >/dev/null 2>&1 \
    || fail "run did not exit cleanly (dialog hang? exit $?)"

[ -f "$log" ] || fail "no log written ($log)"
grep -q "num=18 .*InvAct=18.* -> TRUE" "$log" \
    || fail "give did not register (skipmodals skipped the give, not just the dialog)"

pass "give completes headlessly with skipmodals (clean exit + use registered)"
