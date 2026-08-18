#!/usr/bin/env bash
# ui menu-options @ 1280x720: the first mode the resolution menu offers where the
# framebuffer is taller than the 480-line authored canvas, so UI_VCENTER_OFS is
# non-zero and the UI floats vertically as well as horizontally. Asserts the
# centred 640x480 crop of the capture is byte-identical to the 640 golden.
#
# Pairs with test_ui_menu_options.sh (640 pinned) and _wide (768x480, which only
# moves the UI horizontally). Until this test the goldens stopped at 768: the
# helper took a resolution argument, nothing passed it one above 480 tall, and a
# regression in the vertical float would have shown up only in manual play.
#
# menu-main is deliberately not covered at any width above 640. It renders over
# the live sky scene rather than --black-bg, and a wider frame shows more of that
# scene: the crop differs by ~133k pixels at 768 before anything about the UI has
# moved. Only the cleanroom capture can be compared this way.
TESTNAME=ui_menu_options_hd
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_options_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

# Same plasma-strip exclusion as test_ui_menu_options.sh, in the 640 golden's
# coordinates: the centred crop has already put both images on one origin.
ui_compare_wide "1280x720" --exclude 46,71,549,49 "--black-bg menu-options" "$GOLDEN"
