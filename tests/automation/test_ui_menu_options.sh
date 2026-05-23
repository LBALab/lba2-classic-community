#!/usr/bin/env bash
# ui menu-options: capture the in-game options/pause menu (the screen
# pressing ESC during gameplay opens), byte-compare to the committed
# golden.  Fixture: Anon1 (Citadel Island scene shaded behind the menu).
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_menu_options.sh
TESTNAME=ui_menu_options
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_options_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "menu-options" "$GOLDEN"
