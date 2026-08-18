#!/usr/bin/env bash
# ui menu-options @ 1920x1080: the tallest framebuffer the engine accepts
# (ADELINE_MAX_Y_RES, which sizes TabOffLine[]), and so the largest vertical
# float the authored canvas ever sits at: UI_VCENTER_OFS is 300 here against
# 120 at 720. Asserts the centred 640x480 crop of the capture is byte-identical
# to the 640 golden.
#
# Pairs with test_ui_menu_options_hd.sh. Two rungs rather than one because this
# is the boundary: a rounding or clamp error in the float that stays invisible
# at 720 has 300 lines to show itself in here, and this is the mode where a row
# table overrun would land outside the buffer rather than inside the next one.
TESTNAME=ui_menu_options_uhd
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_options_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

# Same plasma-strip exclusion as test_ui_menu_options.sh, in the 640 golden's
# coordinates: the centred crop has already put both images on one origin.
ui_compare_wide "1920x1080" --exclude 46,71,549,49 "--black-bg menu-options" "$GOLDEN"
