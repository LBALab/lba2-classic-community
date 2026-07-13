#!/usr/bin/env bash
# The CLI contract that automation depends on (#433). Every case here guards against a
# run that silently does the wrong thing and still exits 0:
#
#   --help          prints usage and exits 0 WITHOUT booting the game
#   unknown flag    exits non-zero instead of booting with the flag silently ignored
#   missing value   exits non-zero instead of dropping the flag ("--tick" with no count)
#   flag-as-value   exits non-zero instead of swallowing it ("--exec-at 5 --tick 30" used
#                   to parse cmds="--tick", eat the 30, and tick zero times)
#   --exec-at       runs a command after a scene change has settled, which --exec cannot
#   --no-autosave   blocks autosaves but NOT an explicit save: the console's `savebug`
#                   writes to the bugs dir and must keep working
TESTNAME=cli_contract
. "$(dirname "$0")/lib.sh"
precheck

# --- --help: exits 0, prints usage, does not open a window ---------------------
out="$(timeout 10 "$LBA2_BIN" --help 2>/dev/null)" || fail "--help exited non-zero"
case "$out" in
*Usage:*--headless*) ;;
*) fail "--help did not print usage (got: $(echo "$out" | head -1))" ;;
esac

# --- unknown flag: refuses to run ----------------------------------------------
if timeout 10 "$LBA2_BIN" --resolutionn 1280x720 --tick 1 --exit >/dev/null 2>&1; then
    fail "unknown flag '--resolutionn' was accepted (exit 0) instead of rejected"
fi

# --- missing value: refuses to run ---------------------------------------------
if timeout 10 "$LBA2_BIN" --headless --tick >/dev/null 2>&1; then
    fail "'--tick' with no value was accepted instead of rejected"
fi

# --- a flag where a value belongs: refuses to run -------------------------------
if timeout 10 "$LBA2_BIN" --headless --exec-at 5 --tick 30 --exit >/dev/null 2>&1; then
    fail "'--exec-at 5 --tick 30' was accepted; --tick would be swallowed as the command"
fi

# --- mixed-case data-dir flag still works (RES_DISCOVERY matches case-insensitively) ---
if ! timeout 10 "$LBA2_BIN" --GAME-DIR "$LBA2_GAME_DIR" --help >/dev/null 2>&1; then
    fail "'--GAME-DIR' rejected; RES_DISCOVERY accepts it case-insensitively"
fi

# --- a --game-dir that doesn't hold the data must fail, not boot another install --------
# Discovery falls back to the persisted / auto-discovered path, so an unusable --game-dir
# would boot a DIFFERENT install and say so only in the banner: an A/B against assets you
# never asked for, exiting 0.
if timeout 30 "$LBA2_BIN" --game-dir /nonexistent/path --headless --tick 1 --exit >/dev/null 2>&1; then
    fail "an unusable --game-dir was accepted; the run booted some other install"
fi

# --- a batch run must never block on the folder picker ------------------------------
# --headless brings SDL up on the dummy video driver, so SDL_INIT_VIDEO succeeds and the
# modal picker really does open, with nobody to answer it.
timeout 30 "$LBA2_BIN" --pick-game-dir --headless --tick 1 --exit >/dev/null 2>&1
[ $? -eq 124 ] && fail "--pick-game-dir hung a headless run on the modal folder picker"

need_save

json="$(mktemp)"
trap 'rm -f "$json"' EXIT

# --- --exec-at fires after the cube change has landed ---------------------------
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

# --- --no-autosave must not break an explicit save (console `savebug`) -----------
# ctl() passes --no-autosave for every test, so gating SaveGame() would silently turn
# `savebug` into a no-op across the whole suite.
bugs_dir="${XDG_DATA_HOME:-$HOME/.local/share}/Twinsen/LBA2/save/bugs"
bug_file="$bugs_dir/clicontract.lba"
rm -f "$bug_file"
ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --exec "savebug clicontract" --tick 5 --exit \
    >/dev/null 2>&1 || fail "savebug run exited non-zero"
[ -f "$bug_file" ] || fail "savebug wrote nothing under --no-autosave (expected $bug_file)"
rm -f "$bug_file"

pass "--help/unknown/missing-value/flag-as-value all rejected; --exec-at landed ($hx,$hz); savebug survives --no-autosave"
