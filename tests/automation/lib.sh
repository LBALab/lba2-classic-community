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
    { [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ] \
        || [ "${SDL_VIDEODRIVER:-}" = "dummy" ]; } \
        || skip "no display (engine needs a window, or set SDL_VIDEODRIVER=dummy)"
}

need_save() {
    [ -n "$LBA2_TEST_SAVE" ] && [ -f "$LBA2_TEST_SAVE" ] || skip "no save fixture (set LBA2_TEST_SAVE)"
}

# ctl <args...> — run the harness with null audio and a hard timeout.
ctl() {
    SDL_AUDIODRIVER=dummy timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" "$@"
}

# ctl_headless <args...> — same as ctl but also pins SDL_VIDEODRIVER=dummy.
# Used by UI capture tests where the committed golden was rendered under the
# dummy video driver, so the test must too for the byte-identical compare.
ctl_headless() {
    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" "$@"
}

# ui_compare <verb-args...> <golden-path>
# Runs `ctl_headless --load $LBA2_TEST_SAVE --exec "ui <verb-args>" ...` writing
# the capture to a temp file, then compares sha256 against the committed golden.
# Set LBA2_UI_REGEN=1 to copy the capture over the golden instead of comparing.
ui_compare() {
    local args="" golden="" out= shaA shaB
    while [ $# -gt 1 ]; do args="${args:+$args }$1"; shift; done
    golden="$1"
    out="$(mktemp -t "${TESTNAME:-ui}.XXXXXX.png")"
    # the verb takes the capture path as its last arg
    ctl_headless --load "$LBA2_TEST_SAVE" \
        --exec "ui $args $out" --fixed-dt 16 --tick 200 --exit >/dev/null 2>&1 \
        || { rm -f "$out"; fail "verb 'ui $args' returned non-zero ($?)"; }
    [ -f "$out" ] || fail "no capture written for 'ui $args'"
    if [ "${LBA2_UI_REGEN:-}" = "1" ]; then
        mkdir -p "$(dirname "$golden")"
        cp "$out" "$golden"
        rm -f "$out"
        pass "regenerated golden: $(basename "$golden")"
    fi
    [ -f "$golden" ] || { rm -f "$out"; fail "golden not committed yet: $golden"; }
    shaA=$(sha256sum "$out" | awk '{print $1}')
    shaB=$(sha256sum "$golden" | awk '{print $1}')
    if [ "$shaA" = "$shaB" ]; then
        rm -f "$out"
        pass "ui $args matches golden ($(basename "$golden"))"
    else
        # leave $out behind under /tmp for inspection
        fail "ui $args diverges from golden ($shaA vs $shaB; capture at $out)"
    fi
}

# jget <json-file> <python-expr over `d`> — print a field, e.g. jget s.json "d['scene']['cube']"
jget() {
    python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print(eval(sys.argv[2]))" "$1" "$2"
}

# projrec_compare <out-file> <golden-path>
# Compares the captured projection hash to a committed golden, ignoring
# leading `#`-prefixed comment lines (so the header text can evolve without
# breaking tests — only the hashed data lines matter for regression).
# Set LBA2_PROJECTION_REGEN=1 to copy the capture over the golden instead.
projrec_compare() {
    local out="$1" golden="$2"
    [ -f "$out" ] || fail "no hash file written: $out"
    if [ "${LBA2_PROJECTION_REGEN:-}" = "1" ]; then
        mkdir -p "$(dirname "$golden")"
        cp "$out" "$golden"
        rm -f "$out"
        pass "regenerated golden: $(basename "$golden")"
    fi
    [ -f "$golden" ] || { rm -f "$out"; fail "golden not committed yet: $golden"; }
    if diff <(grep -v '^#' "$out") <(grep -v '^#' "$golden") >/dev/null; then
        rm -f "$out"
        pass "projection hash matches golden ($(basename "$golden"))"
    else
        fail "projection hash diverges from golden — see $out vs $golden"
    fi
}
