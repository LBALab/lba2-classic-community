#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Run the engine on an attached ARM device, as if it were the local binary.
#
# Point LBA2_BIN at this and tests/automation runs against the device with no
# change to lib.sh and no change to any fixture:
#
#   export LBA2_BIN=$PWD/scripts/dev/android-lba2cc.sh
#   export LBA2_ANDROID_ENGINE=$PWD/out/build/android_arm64_cli/SOURCES/lba2cc
#   bash tests/automation/test_negative.sh
#
# lib.sh's precheck only requires LBA2_BIN to be an executable file, so this is
# the one seam that also catches the six fixtures that bypass the ctl helpers
# and invoke "$LBA2_BIN" directly.
#
# HOW PATHS CROSS. Fixtures pass HOST paths on argv and then read the results
# back locally. Rather than teach this script which arguments are paths, mirror
# three host roots onto the device and rewrite any argument that starts with
# one:
#
#   $TMPDIR          -> <device>/tmp     pushed before the run, pulled after
#   $LBA2_GAME_DIR   -> <device>/game    staged once
#   <repo root>      -> <device>/repo    staged once
#
# Plain longest-prefix substitution on the whole argument, which is why it also
# reaches paths buried inside an --exec string. A table of "which flag takes a
# path" could not: 14 UI fixtures put their output path inside --exec, and
# --load and --record accept a bare NAME resolved in the user directory, so a
# boolean "this value is a path" column would be wrong on day one. Pulling the
# whole directory back also brings .rec siblings, adeline.log and lba2.cfg
# without knowing they exist.
#
# Env:
#   LBA2_ANDROID_ENGINE  the arm64 lba2cc built with -DLBA2_ANDROID_EXECUTABLE=ON
#   LBA2_ANDROID_DRYRUN  print the translated invocation and exit, touching no
#                        device. The translation is the part worth testing on
#                        its own, so it is testable on its own.
# ---------------------------------------------------------------------------
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/dev/android-lib.sh
. "$HERE/android-lib.sh"

REPO_ROOT="$(cd "$HERE/../.." && pwd)"
ENGINE="${LBA2_ANDROID_ENGINE:-$REPO_ROOT/out/build/android_arm64_cli/SOURCES/lba2cc}"
DRYRUN="${LBA2_ANDROID_DRYRUN:-}"

DEV_TMP="$ANDROID_DEVICE_DIR/tmp"
DEV_GAME="$ANDROID_DEVICE_DIR/game"
DEV_REPO="$ANDROID_DEVICE_DIR/repo"
DEV_SAVE="$ANDROID_DEVICE_DIR/save"

# The host roots. Resolve to physical paths: a fixture may hand us a path under
# a symlinked TMPDIR, and a prefix that does not match textually silently fails
# to translate, leaving a host path on the device where it cannot resolve.
host_root() { [[ -n "${1:-}" && -e "$1" ]] && (cd "$1" && pwd -P); }

HOST_TMP="$(host_root "${TMPDIR:-/tmp}")"
HOST_GAME="$(host_root "${LBA2_GAME_DIR:-}")"
HOST_REPO="$REPO_ROOT"

# A fourth root, because the save fixture is not under any of the other three.
# lib.sh resolves LBA2_TEST_SAVE from $LBA2_GAME_DIR/../SAVE, a sibling of the
# data directory, so 17 fixtures pass a path that no other rule can reach.
HOST_SAVE=""
if [[ -n "${LBA2_TEST_SAVE:-}" ]]; then
    HOST_SAVE="$(host_root "$(dirname "$LBA2_TEST_SAVE")")"
elif [[ -n "$HOST_GAME" ]]; then
    HOST_SAVE="$(host_root "$HOST_GAME/../SAVE")"
fi

# Substring replacement, not prefix matching, and every occurrence rather than
# the first. An --exec value is a whole console command line with the path in
# the middle of it ("ui inventory /tmp/xxx/shot.png"), so an argument that only
# CONTAINS a host root still has to be rewritten. Prefix matching would leave
# that path spelled for the host, on the device, where it cannot resolve, and
# the fixture would fail somewhere far from the cause.
#
# Longest host root first: with a game dir nested inside the repo, rewriting the
# repo root first would leave the remainder pointing into the wrong tree.
declare -a ROOT_PAIRS=()
build_root_pairs() {
    local pair
    for pair in "$HOST_TMP|$DEV_TMP" "$HOST_GAME|$DEV_GAME" "$HOST_REPO|$DEV_REPO" \
        "$HOST_SAVE|$DEV_SAVE"; do
        [[ -z "${pair%%|*}" ]] && continue
        ROOT_PAIRS+=("${#pair} $pair")
    done
    mapfile -t ROOT_PAIRS < <(printf '%s\n' "${ROOT_PAIRS[@]}" | sort -rn | cut -d' ' -f2-)
}
build_root_pairs

# Collapse "/seg/../" textually before matching. lib.sh finds the save fixture at
# $LBA2_GAME_DIR/../SAVE, so an unnormalised --load argument still starts with
# the game root and translates to <device>/game/../SAVE, which resolves OUTSIDE
# the mirror to a path that was never pushed. Purely textual, so it needs no
# filesystem and works on a path inside an --exec string as readily as on a bare
# argument.
normalize_dotdot() {
    local a="$1" prev=""
    while [[ "$a" != "$prev" ]]; do
        prev="$a"
        a="$(printf '%s' "$a" | sed -E 's#/[^/ ]+/\.\./#/#g')"
    done
    printf '%s' "$a"
}

translate_arg() {
    local a pair host dev
    a="$(normalize_dotdot "$1")"
    for pair in "${ROOT_PAIRS[@]}"; do
        host="${pair%%|*}"
        dev="${pair##*|}"
        a="${a//"$host"/"$dev"}"
    done
    printf '%s' "$a"
}

TRANSLATED=()
for arg in "$@"; do
    TRANSLATED+=("$(translate_arg "$arg")")
done

# The user dir and game dir travel as environment, not as --user-dir/--game-dir.
# Both reach the same directory, but the engine reports WHERE the setting came
# from, and test_user_dirs.sh and test_cli_override_persistence.sh assert on
# that. Promoting an env var to a flag would change the reported source and fail
# a fixture for a reason that has nothing to do with the device.
DEV_USER_DIR="$(translate_arg "${LBA2_USER_DIR:-$HOST_TMP/user}")"

quoted=""
for a in "${TRANSLATED[@]}"; do
    quoted+=" $(printf '%q' "$a")"
done

if [[ -n "$DRYRUN" ]]; then
    echo "engine:   $ANDROID_DEVICE_DIR/bin/lba2cc"
    echo "user-dir: $DEV_USER_DIR"
    echo "game-dir: $DEV_GAME"
    echo "argv:    $quoted"
    exit 0
fi

android_probe || exit 1

if [[ ! -x "$ENGINE" ]]; then
    echo "android-lba2cc: no engine at $ENGINE" >&2
    echo "  build one with -DLBA2_ANDROID_EXECUTABLE=ON, or set LBA2_ANDROID_ENGINE" >&2
    exit 1
fi

echo "android-lba2cc: staging and running is not wired up yet; use --dry-run" >&2
exit 1
