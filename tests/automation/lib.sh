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

# A folder of this test's own, unless the test named one first.
#
# The engine reads its settings from the player's lba2.cfg with the install's stacked
# underneath, so a fixture that doesn't isolate is comparing against whatever the
# developer plays with. Every key a surface displays or a camera consults is then an
# input the golden never declared: FollowCamera and DetailLevel move the projection
# corpus, DisplayFullScreen and VSync move the Display submenu. The goldens are all
# reproducible from a fresh folder, which is the state they were captured in, so
# handing each test one is what makes them mean anything on someone else's machine.
#
# Isolating here rather than per test because the rule only holds if it cannot be
# forgotten: a new fixture is isolated by existing, not by remembering to say so.
if [ -z "${LBA2_USER_DIR:-}" ]; then
    LBA2_USER_DIR="$(mktemp -d -t "lba2-${TESTNAME:-auto}-XXXXXX")"
    _LIB_OWNS_USER_DIR="$LBA2_USER_DIR"
fi
export LBA2_USER_DIR

# A test that installs its own EXIT trap replaces this one, in which case the folder
# is left under TMPDIR rather than removed. That is the harmless direction: those
# tests name their own folder anyway, so there is usually nothing here to remove, and
# a leaked temp dir costs less than a cleanup that fought the test's own restore.
_lib_cleanup() {
    local rc=$?
    [ -n "${_LIB_OWNS_USER_DIR:-}" ] && rm -rf "$_LIB_OWNS_USER_DIR"
    return $rc
}
trap _lib_cleanup EXIT

SKIP_RC=77 # autotools convention: a skipped test

# user_dir -- the folder the engine writes to for this run: saves, lba2.cfg,
# adeline.log, last_game_dir.txt. LBA2_USER_DIR when a test has isolated itself,
# otherwise the per-user default SDL picks.
#
# Tests that reach into that folder ask here rather than rebuilding the platform
# path, which they used to do by knowing the org/app SDL_GetPrefPath appends.
user_dir() {
    if [ -n "${LBA2_USER_DIR:-}" ]; then
        printf '%s\n' "${LBA2_USER_DIR%/}"
    else
        case "$(uname -s)" in
            Darwin) printf '%s\n' "$HOME/Library/Application Support/Twinsen/LBA2" ;;
            *)      printf '%s\n' "${XDG_DATA_HOME:-$HOME/.local/share}/Twinsen/LBA2" ;;
        esac
    fi
}

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

# ctl <args...> — run the harness headless, with a hard timeout.
#
# --headless (not just SDL_AUDIODRIVER=dummy) because a visible window pauses the game when
# it loses focus: a windowed test gets a stopped clock the moment you tab away, and several
# in parallel fight over focus, so results stop reproducing. It also implies --no-audio.
#
# --no-autosave because an in-game scene transition writes autosave.lba: without it, merely
# running the suite mutates the developer's own save files.
ctl() {
    timeout "$LBA2_TEST_TIMEOUT" "$LBA2_BIN" --headless --no-autosave "$@"
}

# ctl_headless <args...> — same as ctl but also pins SDL_VIDEODRIVER=dummy,
# Language to English, the render resolution to 640x480, and vsync on. Used
# by UI capture tests where the committed goldens were rendered under the
# dummy video driver, English UI, the engine's default 640x480 framebuffer
# and the engine's default vsync setting.
#
# --language English: without this, tests fail on machines whose local
# lba2.cfg has any other Language: key (typically Français, which is also
# the engine's default if no key is present). English matches the in-repo
# LBA2.CFG default.
#
# --resolution 640x480: pins the render resolution so cfg's ResolutionX/Y
# (which now persists per the runtime-resolution-switch work in PRs
# #237-#241/#244) doesn't shift the framebuffer dimensions out from under
# the goldens. Without this, a developer who switched to 768x480 in their
# own session would have ResolutionX=768 in cfg and all UI goldens would
# fail with mismatched dimensions on their next test run.
#
# --no-audio skips the SDL audio subsystem entirely so the dummy driver's
# nanosleep pacing (~74% of sys time on WSL2 per strace -c) doesn't run.
# Goldens are rendering-only (no audio sampling), so dropping audio init
# doesn't affect them — verified by re-running the suite with this in
# place and confirming all eight ui_* goldens stay byte-identical.
#
# --vsync on: the Display submenu prints the setting as one of its rows, so
# without this the capture reads the developer's own cfg and a machine with
# VSync: 0 fails ui_display on a golden that is correct. Pinning the setting
# doesn't slow the run down, because a --exit run drops the presentation cap
# either way (Control_Begin).
ctl_headless() {
    timeout "$LBA2_TEST_TIMEOUT" \
        "$LBA2_BIN" --headless --no-autosave --language English --resolution 640x480 \
        --vsync on "$@"
}

# ctl_headless_cfg_driven <args...> — same as ctl_headless but does NOT
# pin --resolution. For tests that verify lba2.cfg's ResolutionX/Y is the
# actual boot-resolution source (test_res_switch_cfg.sh). Every other
# test should use ctl_headless so the boot resolution is deterministic.
ctl_headless_cfg_driven() {
    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy timeout "$LBA2_TEST_TIMEOUT" \
        "$LBA2_BIN" --language English --no-audio "$@"
}

# ctl_headless_at <WxH> <args...> — same as ctl_headless but renders at
# the given resolution instead of 640x480. Used by ui_compare_wide to
# capture a wider frame for centred-crop differential comparison.
ctl_headless_at() {
    local res="$1"; shift
    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy timeout "$LBA2_TEST_TIMEOUT" \
        "$LBA2_BIN" --language English --no-audio --resolution "$res" --vsync on "$@"
}

# seed_menu_save_dir <user-dir> <fresh|returning> [save-file]
# The boot main menu is assembled from the save directory, not from --load:
# BuildGameMainMenu (GAMEMENU.CPP) adds "Resume Game" when save/current.lba
# exists, which also shifts the whole menu's vertical centre from 275 to 335,
# and adds "Load Game" when IsExistSavedGame() finds any named save. A test
# capturing that menu therefore owns the save directory, or it renders whichever
# shape the developer's own saves happen to produce and compares it against a
# golden captured on somebody else's disk.
#
# Seeds a user directory of the test's own for one of the two states worth a
# golden: `fresh` is what a new install shows (New / Options / Quit, no
# thumbnail), `returning` is the full menu (Resume / New / Load / Options /
# Quit, thumbnail from current.lba). The caller exports LBA2_USER_DIR and owns
# the cleanup.
seed_menu_save_dir() {
    local dir="$1/save" mode="$2" save="${3:-$LBA2_TEST_SAVE}"
    mkdir -p "$dir"
    if [ "$mode" = "returning" ]; then
        # current.lba is the thumbnail and the Resume row; the named save is the
        # only thing IsExistSavedGame looks for, and its name does not matter.
        cp "$save" "$dir/current.lba"
        cp "$save" "$dir/Anon1.LBA"
    fi
}

# ui_compare <verb-args...> <golden-path>
# Runs `ctl_headless --load $LBA2_TEST_SAVE --exec "ui <verb-args>" ...` writing
# the capture to a temp file, then compares sha256 against the committed golden.
# Set LBA2_UI_REGEN=1 to copy the capture over the golden instead of comparing.
#
# An empty LBA2_TEST_SAVE runs without --load, for the one surface whose subject
# is the absence of a save: the main menu a new install shows.
ui_compare() {
    local args="" golden="" out= shaA shaB rc=0
    while [ $# -gt 1 ]; do args="${args:+$args }$1"; shift; done
    golden="$1"
    out="$(mktemp -t "${TESTNAME:-ui}.XXXXXX.png")"
    # the verb takes the capture path as its last arg
    if [ -n "${LBA2_TEST_SAVE:-}" ]; then
        ctl_headless --load "$LBA2_TEST_SAVE" \
            --exec "ui $args $out" --fixed-dt 16 --tick 200 --exit >/dev/null 2>&1 || rc=$?
    else
        ctl_headless \
            --exec "ui $args $out" --fixed-dt 16 --tick 200 --exit >/dev/null 2>&1 || rc=$?
    fi
    [ "$rc" = 0 ] || { rm -f "$out"; fail "verb 'ui $args' returned non-zero ($rc)"; }
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

# ui_compare_wide <WxH> <verb-args...> <golden-path>
# Renders the same ui capture at a wider resolution and pixel-compares
# the centred (golden_w x golden_h) crop of the wide capture to the
# 640 golden. Strict: byte-identical in the crop region or fail.
#
# Only meaningful for surfaces with a cleanroom golden (captured with
# `ui --black-bg`) — otherwise the legitimate FoV-sensitive scene render
# at the wider width will appear as a diffuse diff in the crop. See
# WIDESCREEN.md "Testing posture" for the surface-by-surface picture.
#
# Requires Pillow (`pip install Pillow`); skips if missing.
# LBA2_UI_REGEN is a no-op — the 640 golden is the single source of
# truth, there is no per-width golden to regenerate.
ui_compare_wide() {
    local res="$1"; shift
    local args="" golden="" out= py_rc
    while [ $# -gt 1 ]; do args="${args:+$args }$1"; shift; done
    golden="$1"
    python3 -c "from PIL import Image" 2>/dev/null \
        || skip "Pillow not installed (pip install Pillow)"
    [ -f "$golden" ] || skip "640 golden missing — run the 640 test first: $golden"
    out="$(mktemp -t "${TESTNAME:-ui}.XXXXXX.png")"
    ctl_headless_at "$res" --load "$LBA2_TEST_SAVE" \
        --exec "ui $args $out" --fixed-dt 16 --tick 200 --exit >/dev/null 2>&1 \
        || { rc=$?; rm -f "$out"; fail "verb 'ui $args' at $res returned non-zero ($rc)"; }
    [ -f "$out" ] || fail "no capture written for 'ui $args' at $res"
    python3 - "$golden" "$out" <<'PY'
import sys
from PIL import Image
golden = Image.open(sys.argv[1]).convert("RGB")
capture = Image.open(sys.argv[2]).convert("RGB")
gw, gh = golden.size
cw, ch = capture.size
if cw < gw or ch < gh:
    sys.stderr.write(f"capture {cw}x{ch} smaller than golden {gw}x{gh}\n")
    sys.exit(2)
dx, dy = (cw - gw) // 2, (ch - gh) // 2
cropped = capture.crop((dx, dy, dx + gw, dy + gh))
gpx = golden.tobytes()
cpx = cropped.tobytes()
if gpx == cpx:
    sys.exit(0)
diff = sum(1 for i in range(0, len(gpx), 3) if gpx[i:i+3] != cpx[i:i+3])
sys.stderr.write(f"{diff} pixels differ in centred {gw}x{gh} crop (offset {dx},{dy})\n")
sys.exit(1)
PY
    py_rc=$?
    if [ "$py_rc" = 0 ]; then
        rm -f "$out"
        pass "ui $args matches centred crop of $res capture ($(basename "$golden"))"
    else
        fail "ui $args at $res diverges from 640 golden in centred crop (capture at $out)"
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
