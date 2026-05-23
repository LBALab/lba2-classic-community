#!/usr/bin/env bash
# ui menu-main: capture the boot-time main game menu (Resume / New / Load /
# Options / Quit over the cloudy-sky background), byte-compare to the
# committed golden.  Fixture: Anon1 (so the Resume option is selectable
# and the save thumbnail shows at top).
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_menu_main.sh
TESTNAME=ui_menu_main
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_main_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "menu-main" "$GOLDEN"
