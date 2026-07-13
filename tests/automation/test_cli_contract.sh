#!/usr/bin/env bash
# The CLI contract that automation depends on (#433):
#
#   --help          prints usage and exits 0 WITHOUT booting the game
#   unknown flag    exits non-zero instead of booting with the flag silently ignored
#   --exec-at       runs a command after a scene change has settled, which --exec cannot
#
# The first two need no retail data (they exit before SDL init), so they run even on a
# machine with no game data. --exec-at needs a scene, so it precheck-skips like the rest.
TESTNAME=cli_contract
. "$(dirname "$0")/lib.sh"

# --- --help: exits 0, prints usage, does not open a window ---------------------
out="$(timeout 10 "$LBA2_BIN" --help 2>/dev/null)" || fail "--help exited non-zero"
case "$out" in
*Usage:*--headless*) ;;
*) fail "--help did not print usage (got: $(echo "$out" | head -1))" ;;
esac

# --- unknown flag: refuses to run ----------------------------------------------
# A silently-ignored typo is the failure mode this guards: the run would otherwise
# proceed on a default and hand back a confident, wrong answer.
if timeout 10 "$LBA2_BIN" --resolutionn 1280x720 --tick 1 --exit >/dev/null 2>&1; then
    fail "unknown flag '--resolutionn' was accepted (exit 0) instead of rejected"
fi

# --- --exec-at: fires after the cube change has landed --------------------------
precheck

json="$(mktemp)"
trap 'rm -f "$json"' EXIT

# `--exec "cube 154; teleport actor 3"` would teleport in the OLD cube (the change lands
# next frame) and the hero would end up at 154's spawn. Scheduling the teleport for a
# later tick puts it in the new scene, where actor 3 is a door the hero walks through.
ctl --fixed-dt 16 --exec "cube 154" --exec-at 10 "teleport actor 3" \
    --tick 60 --dump-state "$json" --exit >/dev/null 2>&1 ||
    fail "non-zero exit ($?) — hang or crash"

hx=$(jget "$json" "d['hero']['x']")
hz=$(jget "$json" "d['hero']['z']")

# Cube 154's spawn is (5888, 1792): if the teleport had been lost to the race, the hero
# would still be sitting there.
if [ "$hx" = "5888" ] && [ "$hz" = "1792" ]; then
    fail "hero still at cube 154's spawn ($hx,$hz) — --exec-at did not fire in the new scene"
fi

pass "--help exits 0, unknown flag rejected, --exec-at landed (hero at $hx,$hz)"
