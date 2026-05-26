#!/usr/bin/env bash
# ui menu-main: capture the boot-time main game menu (Resume / New / Load /
# Options / Quit over the cloudy-sky background), byte-compare to the
# committed golden.  Fixture: Anon1 (so the Resume option is selectable
# and the save thumbnail shows at top).
#
# The thumbnail at the top of the main menu is read by the engine from
# ~/.local/share/Twinsen/LBA2/save/current.lba — *not* from the --load
# argument.  with_menu_main_fixture copies the corpus Anon1.LBA into
# current.lba for the test run and restores the original on exit; without
# this the test result depends on the developer's last play session.
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_menu_main.sh
TESTNAME=ui_menu_main
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_main_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

with_menu_main_fixture 'ui_compare "menu-main" "$GOLDEN"'
