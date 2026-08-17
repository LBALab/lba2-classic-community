#!/usr/bin/env bash
# ui menu-main on a new install: New Game / Options Menu / Quit, no Resume row,
# no Load row, no save thumbnail, and the menu sitting at its higher vertical
# centre (275 rather than the 335 a Resume row moves it to).
#
# The first screen anybody ever sees, and nothing covered it: every ui_* golden
# is captured with a save in play, so the whole no-save path through
# BuildGameMainMenu went unrendered by the suite. It is also the shorter of the
# two menus and the only one drawn without a thumbnail, which is where a change
# to the menu's own centring shows up first.
#
# No --load here, deliberately: loading a save is the thing this state is
# defined by the absence of. ui_compare drops --load when LBA2_TEST_SAVE is
# empty.
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_menu_main_fresh.sh
TESTNAME=ui_menu_main_fresh
. "$(dirname "$0")/lib.sh"
precheck

GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_main_fresh.png"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
export LBA2_USER_DIR="$tmp/user"
LBA2_TEST_SAVE=""
seed_menu_save_dir "$LBA2_USER_DIR" fresh

# Plasma strip excluded, as in test_ui_menu_main.sh; four rows higher here,
# because a fresh install's menu has one entry fewer and the panel sits up.
ui_compare --exclude 46,170,549,49 "menu-main" "$GOLDEN"
