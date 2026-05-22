# Shared helpers for the CLI control-harness tests.
#
# These drive the real engine binary, so they need retail game data and a display.
# They are local-only (not part of host-only `make test`) and skip cleanly when the
# environment isn't there. Override any of these with env vars:
#
#   LBA2_BIN          path to the built binary (default: autodiscovered under build/)
#   LBA2_GAME_DIR     retail data dir containing LBA2.HQR (required)
#   LBA2_TEST_SAVE    a .lba/.LBA save for --load tests (default: first under <data>/../SAVE)
#   LBA2_TEST_TIMEOUT per-run timeout in seconds (default: 90)
#
# A hung run (timeout) is reported as a failure, never a blocked terminal.

set -u

_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
REPO="$(cd "$_LIB_DIR/../.." && pwd)"

: "${LBA2_BIN:=}"
if [ -z "$LBA2_BIN" ]; then
    for _c in "$REPO/build/SOURCES/lba2cc" "$REPO/build/SOURCES/lba2" \
              "$REPO"/out/build/*/SOURCES/lba2cc "$REPO"/out/build/*/SOURCES/lba2; do
        if [ -x "$_c" ]; then LBA2_BIN="$_c"; break; fi
    done
fi

: "${LBA2_GAME_DIR:=}"
export LBA2_GAME_DIR

: "${LBA2_TEST_SAVE:=}"
if [ -z "$LBA2_TEST_SAVE" ] && [ -n "$LBA2_GAME_DIR" ]; then
    for _s in "$LBA2_GAME_DIR/../SAVE/"*.LBA "$LBA2_GAME_DIR/../SAVE/"*.lba; do
        if [ -f "$_s" ]; then LBA2_TEST_SAVE="$_s"; break; fi
    done
fi

: "${LBA2_TEST_TIMEOUT:=90}"

SKIP_RC=77 # autotools convention: a skipped test

skip() { echo "SKIP: ${TESTNAME:-test}: $*"; exit $SKIP_RC; }
fail() { echo "FAIL: ${TESTNAME:-test}: $*"; exit 1; }
pass() { echo "PASS: ${TESTNAME:-test}${1:+ — $1}"; exit 0; }

precheck() {
    [ -n "$LBA2_BIN" ] && [ -x "$LBA2_BIN" ] || skip "no binary (build it, or set LBA2_BIN)"
    [ -n "$LBA2_GAME_DIR" ] || skip "LBA2_GAME_DIR unset"
    [ -e "$LBA2_GAME_DIR" ] || skip "LBA2_GAME_DIR not found: $LBA2_GAME_DIR"
    { [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; } || skip "no display (engine needs a window)"
}

need_save() {
    [ -n "$LBA2_TEST_SAVE" ] && [ -f "$LBA2_TEST_SAVE" ] || skip "no save fixture (set LBA2_TEST_SAVE)"
}

# ctl <args...> — run the harness with null audio and a hard timeout.
ctl() {
    SDL_AUDIODRIVER=dummy timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" "$@"
}

# jget <json-file> <python-expr over `d`> — print a field, e.g. jget s.json "d['scene']['cube']"
jget() {
    python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print(eval(sys.argv[2]))" "$1" "$2"
}
