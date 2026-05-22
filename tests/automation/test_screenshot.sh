#!/usr/bin/env bash
# --screenshot: writes a real PNG of the rendered frame. Asserts the PNG magic and a
# nonzero size; best-effort checks the frame isn't a single uniform colour (something
# rendered). --screenshot forces a minimum of 1 tick so a frame is actually drawn.
TESTNAME=screenshot
. "$(dirname "$0")/lib.sh"
precheck
need_save

png="$(mktemp --suffix=.png)"
trap 'rm -f "$png"' EXIT

ctl --load "$LBA2_TEST_SAVE" --tick 2 --screenshot "$png" --exit >/dev/null 2>&1 \
    || fail "non-zero exit ($?)"

[ -s "$png" ] || fail "screenshot is empty/missing"
magic=$(head -c8 "$png" | od -An -tx1 | tr -d ' \n')
[ "$magic" = "89504e470d0a1a0a" ] || fail "not a PNG (magic=$magic)"

# Best-effort: decode and confirm not a single flat colour. Skips the check (does not
# fail) if PIL isn't available.
note=""
python3 - "$png" <<'PY'
import sys
try:
    from PIL import Image
except Exception:
    sys.exit(2)
im = Image.open(sys.argv[1]).convert("RGB")
colors = im.getcolors(maxcolors=1 << 24)
sys.exit(3 if (colors and len(colors) <= 1) else 0)
PY
rc=$?
[ "$rc" = "2" ] && note=" (uniform-colour check skipped: no PIL)"
[ "$rc" = "3" ] && fail "screenshot is a single flat colour — nothing rendered?"

sz=$(wc -c <"$png")
pass "PNG ok, ${sz} bytes$note"
