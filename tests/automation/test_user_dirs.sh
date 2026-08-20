#!/usr/bin/env bash
# The folders the engine writes into have to exist before it writes into them, and it
# has to say so when one cannot be made.
#
# CreateLbaDirectories makes save/, save/shoot/ and recordings/ at boot, and nothing
# looked at whether it managed to. The first thing to notice was a recording: the
# snapshot save failed, and the message named the file rather than the folder that had
# never been made.
#
# Two things the first arm does deliberately, because the suite's own isolation had been
# hiding a Windows failure in both of them at once:
#
#   the folder is named in the platform's own spelling. A path handed to the engine
#   through MSYS2 arrives as `D:/tmp/x`, and forward slashes are a different code path
#   there from the `C:\Users\...` that SDL hands back when nobody names a folder at all.
#
#   the engine is started from the root of the drive that folder is on. The first
#   component of an absolute Windows path is a drive spec, and creating a folder on a
#   drive whose current directory is its own root failed there and only there -- so one
#   drive reproduces what a build tree and a user directory on two drives reproduce by
#   themselves.
#
# Everywhere else both are the identity, and that arm cannot fail on a Linux or macOS
# build however the engine is broken. The second arm is the one that runs everywhere: it
# puts a file where a folder belongs, and pins the report. Deleting the warning fails the
# fixture on any platform, which is what keeps this from being a Windows-only oracle.
TESTNAME=user_dirs

# Named before lib.sh, so the suite's own isolation does not make a second folder that
# nothing uses. Fresh, and empty of every folder this asserts.
#
# `fresh` stays the spelling this shell can test with -d; LBA2_USER_DIR carries the one
# the engine is given. lib.sh is not loaded yet, so this failure reports itself: an empty
# `fresh` would silently move the whole fixture to a folder nothing looks at.
native_path() { # native_path <path> -- the spelling the platform's own API hands back
    if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else printf '%s\n' "$1"; fi
}

fresh="$(mktemp -d -t "lba2-userdirs-XXXXXX")" && [ -d "$fresh" ] || {
    echo "FAIL: user_dirs: mktemp could not make a folder to test in"
    exit 1
}
blocked="$(mktemp -d -t "lba2-blocked-XXXXXX")" && [ -d "$blocked" ] || {
    echo "FAIL: user_dirs: mktemp could not make a folder to test in"
    exit 1
}
LBA2_USER_DIR="$(native_path "$fresh")"
. "$(dirname "$0")/lib.sh"
trap 'rm -rf "$fresh" "$blocked"' EXIT
precheck

# Absolute, because this is the one fixture that runs the engine from another directory:
# a relative LBA2_BIN or LBA2_GAME_DIR passes precheck here and then resolves against the
# wrong folder inside the subshell below, failing for a reason that is not the engine's.
abs_path() { # abs_path <path>
    case "$1" in
    /* | ?:[/\\]*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")" ;;
    esac
}
LBA2_BIN="$(abs_path "$LBA2_BIN")"
LBA2_GAME_DIR="$(abs_path "$LBA2_GAME_DIR")"
export LBA2_GAME_DIR

# The root of the drive the user directory is on, which is where this boots from. `/`
# is not that root under MSYS2: it is the MSYS2 installation, a folder on some drive,
# and booting from a folder that exists asserts nothing.
boot_root() { # boot_root <path>
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$(engine_path "$1" | cut -c1-2)/"
    else
        printf '/\n'
    fi
}

root="$(boot_root "$fresh")"
[ -d "$root" ] || fail "cannot boot from $root: no such directory"

# --- a fresh user directory gets its folders -----------------------------------------
out="$(cd "$root" && ctl --tick 1 --exit 2>&1)" \
    || fail "a boot from $root exited non-zero: $(printf '%s' "$out" | tail -3)"

missing=""
for d in save save/shoot recordings; do
    [ -d "$fresh/$d" ] || missing="$missing $d"
done
[ -z "$missing" ] || fail "booted from $root and made no$missing under $fresh"

case "$out" in
*"could not create"*)
    fail "the boot warned about a folder it did create: $(
        printf '%s' "$out" | grep -m1 'could not create')" ;;
esac

# --- a folder that cannot be made is named at boot -------------------------------------
# A file where save/ belongs, which is what a stray download or a half-restored backup
# leaves behind. The point is the timing: the run has to say it here, with the folder in
# hand, rather than leave it to whatever writes there first and reports a file instead.
: > "$blocked/save" || fail "could not put a file where save/ goes"
blockedout="$(LBA2_USER_DIR="$(native_path "$blocked")" ctl --tick 1 --exit 2>&1)" \
    || fail "a boot with a file where save/ goes exited non-zero: it must survive one"

printf '%s' "$blockedout" | grep -q "could not create.*save" \
    || fail "a boot that could not make save/ said nothing about it"
printf '%s' "$blockedout" | grep -q "could not create.*save.*: ." \
    || fail "the warning named the folder but not the reason: $(
        printf '%s' "$blockedout" | grep -m1 'could not create')"
[ -d "$blocked/recordings" ] \
    || fail "one folder it could not make stopped it making the others"

pass "a boot from $root created save/, save/shoot/ and recordings/ in a fresh user directory; a blocked save/ was named at boot with its reason"
