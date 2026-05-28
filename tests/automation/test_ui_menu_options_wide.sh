#!/usr/bin/env bash
# ui menu-options @ 768x480: re-renders the in-game options/pause menu at the
# Steam Deck render width and asserts the centred 640x480 crop of the capture
# is byte-identical to the existing 640 golden.
#
# Pairs with test_ui_menu_options.sh (640 pinned). Relies on the 640 golden
# being cleanroom (--black-bg, scene-free); strict byte-identical wouldn't
# survive scene bleed at the wider FoV.
TESTNAME=ui_menu_options_wide
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_options_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare_wide "768x480" "--black-bg menu-options" "$GOLDEN"
