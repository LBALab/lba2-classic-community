#!/usr/bin/env bash
# A screen opened on the first frame back from the pause menu shows the scene, not the menu.
#
# Leaving the pause menu re-enters the game loop with the menu still in Log and Screen; the
# scene is drawn only at the end of that first frame. The screens the loop opens (inventory,
# behaviour menu, save, ...) take that picture as their backdrop, so a press landing on the
# first frame opened them over the menu, and a save made there kept the menu as its
# thumbnail. The save is the witness here because its thumbnail reaches the disk.
#
# Which poll is the first frame depends on the menu's own polling, so the F2 press is swept
# across the polls around the menu close; one of them is the first frame (without the fix
# that one stores the menu, about 90% of the thumbnail differs). Each save is compared with
# one made well after the close, where the two differ by under 1% (rain).
# Tier: medium
TESTNAME=first_frame_screens
. "$(dirname "$0")/lib.sh"
precheck
need_save

decompress=""
d="$(dirname "$LBA2_BIN")"
while [ "$d" != "/" ] && [ -n "$d" ]; do
    if [ -x "$d/tools/save_decompress" ]; then decompress="$d/tools/save_decompress"; break; fi
    d="$(dirname "$d")"
done
[ -n "$decompress" ] || skip "no tools/save_decompress next to the binary (LBA2_BUILD_SAVE_TOOLS)"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# save_at <name> <poll>: Esc opens the pause menu, Esc 60 polls later closes it, F2 at
# <poll> opens the save list, Enter on New saves under an automatic name. Prints the file.
save_at() {
    LBA2_USER_DIR="$tmp/$1"
    export LBA2_USER_DIR
    mkdir -p "$LBA2_USER_DIR/save"
    ctl --fixed-dt 16 --load "$LBA2_TEST_SAVE" --exec "input_keyboard 0" \
        --exec-at 30 "key esc 3; key esc 3 60; key 59 2 $2; key enter 6 $(($2 + 60))" \
        --tick 300 --exit >/dev/null 2>&1 || fail "$1: the run failed"
    local f
    for f in "$LBA2_USER_DIR/save"/*; do
        case "${f##*/}" in current.lba | autosave.lba | shoot) ;; *) printf '%s\n' "$f" ;; esac
    done
}

# The share of thumbnail bytes that differ between two saves, 0..1.
thumbdiff() {
    python3 - "$decompress" "$1" "$2" <<'EOF'
import struct, subprocess, sys

def thumb(tool, path):
    d = open(path, 'rb').read()
    if not d[0] & 0x80:
        sys.exit(f"{path}: not a compressed save")
    i = d.index(b'\0', 5) + 1
    size = struct.unpack_from('<i', d, i)[0]
    out = subprocess.run([tool, str(size)], input=d[i + 4:], capture_output=True, check=True).stdout
    return out[:160 * 120]

a, b = thumb(sys.argv[1], sys.argv[2]), thumb(sys.argv[1], sys.argv[3])
print(f"{sum(x != y for x, y in zip(a, b)) / len(a):.4f}")
EOF
}

ref="$(save_at ref 100)"
[ -f "$ref" ] || fail "the reference save (F2 well after the menu closed) was not written"

# F2 while the menu is still up does nothing, and a key already down when the loop starts
# is held back (InitWaitNoInput), so the first poll that saves is the first frame.
first=""
for poll in 58 60 61 62 63 64; do
    s="$(save_at "p$poll" "$poll")"
    if [ ! -f "$s" ]; then
        [ -z "$first" ] || fail "poll $poll: no save, though poll $first saved"
        continue
    fi
    [ -n "$first" ] || first=$poll
    diff="$(thumbdiff "$s" "$ref")" || fail "poll $poll: could not read the thumbnails"
    awk -v d="$diff" 'BEGIN { exit !(d < 0.05) }' ||
        fail "poll $poll: the save's thumbnail differs from the scene by $diff; it was taken over the pause menu"
done
[ -n "$first" ] || fail "no poll in the sweep saved, so the first frame was never reached"
[ "$first" != 58 ] || fail "poll 58 already saved: the sweep starts too late to include the first frame"

pass "the first frame (poll $first) and the polls after it save the scene"
