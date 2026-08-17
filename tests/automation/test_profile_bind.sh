#!/usr/bin/env bash
# A named profile binds to one install, and moving it has to say so.
#
# `--profile gog --game-dir <path>` once, then `--profile gog`: the binding is what
# makes the second line work. A later `--game-dir` is then ambiguous, and the two
# readings are not equally recoverable. Read as "from now on", a one-run override
# silently discards the setup the profile existed to hold, and nothing shows it
# until a later run boots the wrong install. Read as "just this run", a real move
# costs one more flag. So --game-dir runs, and --bind-game-dir moves.
#
# Both directions, because either alone passes on a broken engine: an engine that
# ignored --game-dir once bound would satisfy "the binding did not move", and one
# that never bound anything would satisfy "--bind-game-dir changed it".
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=profile_bind
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# The second install is a tree of symlinks to the real data, so two folders that
# both boot cost nothing and neither depends on the developer having a spare copy.
#
# On Windows it is not free: MSYS2's ln copies unless it is told to make real
# links, so this is a full copy of the game data, and the test pays for it in
# disk and in the seconds that copy takes. It stays a copy rather than becoming
# a second --game-dir the developer has to supply, because a test that needs two
# installs on hand is a test nobody runs.
alt="$tmp/alt-install"
mkdir -p "$alt"
for f in "$LBA2_GAME_DIR"/*; do
    ln -s "$f" "$alt/" 2>/dev/null
done
# An empty tree is not a second install. Where a link can be neither made nor
# faked -- MSYS2 set to winsymlinks:nativestrict without the privilege to create
# one, a TMPDIR on a filesystem without them -- every ln above fails quietly, and
# the run that follows finds no game data to boot. That is an environment this
# test cannot run in, not a binding that went wrong, and reporting it as the
# latter sends the reader after a bug that is not there.
[ -n "$(ls -A "$alt" 2>/dev/null)" ] ||
    skip "cannot copy or link the game data to a second folder"
A="${LBA2_GAME_DIR%/}"
B="$alt"
u="$tmp/user"
# What the engine will call these two folders. The flags below still name them
# the way this shell does, since MSYS2 converts a standalone path argument on
# its way to a native binary; it is only the comparisons that have to know the
# engine answers in its own spelling.
A_SEEN="$(norm_path "$(engine_path "$A")")"
B_SEEN="$(norm_path "$(engine_path "$B")")"

# stdin comes from /dev/null so a child cannot eat the rest of this script.
#
# A run that never started is indistinguishable, downstream, from a binding that
# never happened: every check below reads a file the engine writes, and an engine
# that exited on an unknown flag, missing data or a crash writes none of them. So
# the status is checked here, while the engine can still say why, rather than left
# to surface later as a confident wrong answer about binding.
#
# The engine says why it stopped on one line, either its own argument error or a
# logged one, so that line is what gets quoted. The tail is the fallback for the
# ways a run ends without saying anything: a timeout, or a signal.
run() { # run <extra args...>
    local rc=0 why
    ctl --user-dir "$u" --no-audio --language English --fixed-dt 16 --tick 2 --exit \
        "$@" > "$tmp/run.out" 2>&1 </dev/null || rc=$?
    [ "$rc" -eq 0 ] && return 0
    why=$(tr -d '\r' < "$tmp/run.out" | grep -aE '^(error:|\[ERROR\])' | head -2)
    [ -n "$why" ] || why=$(tr -d '\r' < "$tmp/run.out" | tail -3)
    fail "the run exited $rc: $(printf '%s' "$why" | tr '\n' ' ')"
}
# Which folder the profile is bound to. Compared as a shape rather than a
# spelling: the engine stores the path with a trailing separator, and on Windows
# that separator is a backslash and the line ends CRLF.
bound() { norm_path "$(cat "$u/profiles/p/last_game_dir.txt" 2>/dev/null)"; }
# Which folder the run actually read its assets from, from the boot banner, whose
# shape is `Assets: <path>  (<source>)`. Taken whole rather than as a field: an
# install path with a space in it is ordinary on Windows, and reading one field
# of it silently compares a prefix, which is a pass this test must not give.
assets() {
    norm_path "$(sed -n 's/^Assets: \(.*\)  (.*)$/\1/p' "$u/profiles/p/adeline.log" | head -1)"
}

# --- 1. first use binds ------------------------------------------------------
run --profile p --game-dir "$A"
[ -f "$u/profiles/p/last_game_dir.txt" ] || fail "naming a folder on first use bound nothing"
[ "$(bound)" = "$A_SEEN" ] || fail "first use bound '$(bound)', expected '$A_SEEN'"

# --- 2. a second folder runs, and leaves the binding alone -------------------
run --profile p --game-dir "$B"
[ "$(assets)" = "$B_SEEN" ] ||
    fail "--game-dir did not run against '$B_SEEN' (ran against '$(assets)')"
[ "$(bound)" = "$A_SEEN" ] ||
    fail "--game-dir moved the binding to '$(bound)'; it is for the run only"

# --- 3. asking to move it moves it -------------------------------------------
run --profile p --game-dir "$B" --bind-game-dir
[ "$(bound)" = "$B_SEEN" ] ||
    fail "--bind-game-dir left the binding at '$(bound)', expected '$B_SEEN'"

# --- 4. with nothing to bind, it says so -------------------------------------
# A flag that quietly does nothing is how the setting it was meant to move goes
# unnoticed, which is the failure this whole contract is about.
run --game-dir "$A" --bind-game-dir
grep -q "nothing was bound" "$u/adeline.log" \
    || fail "--bind-game-dir without a profile bound nothing and said nothing"

pass "binds once, runs anywhere, moves when asked"
