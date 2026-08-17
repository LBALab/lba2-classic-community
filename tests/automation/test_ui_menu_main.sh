#!/usr/bin/env bash
# ui menu-main: the boot main menu as a returning player sees it (Resume / New /
# Load / Options / Quit over the cloudy-sky background, save thumbnail on top),
# byte-compared to the committed golden.
#
# This menu is not a fixed screen. BuildGameMainMenu assembles it from the save
# directory: current.lba adds the Resume row and moves the whole menu's vertical
# centre from 275 to 335, and any named save adds the Load row. So the golden is
# a picture of a save directory as much as of a renderer, and the test has to own
# that directory. It used to own half of it, pinning current.lba inside the
# developer's own user folder while leaving the named saves to whatever was
# lying there; on a machine with none, the engine drew a four-row menu against a
# five-row golden and reported it as a rendering divergence.
#
# The other state, the three-row menu a new install shows, has a golden of its
# own in test_ui_menu_main_fresh.sh. Those two bracket what varies here: both
# vertical centres, both thumbnail cases, and the shortest and tallest menus.
# The in-between states (current.lba without named saves, or the reverse) differ
# only in row count and are left out on purpose.
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_menu_main.sh
TESTNAME=ui_menu_main
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/menu_main_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
export LBA2_USER_DIR="$tmp/user"
seed_menu_save_dir "$LBA2_USER_DIR" returning

# The plasma strip across the top of the panel is left out of the comparison: it
# settles at a different state on Windows than on Linux and stays there, so a
# golden that includes it can only ever match one platform. Everything else in
# the frame -- layout, text, colours -- is still compared byte for byte.
# Re-measure with a diff of a failing capture against the golden if the panel moves.
ui_compare --exclude 46,174,549,49 "menu-main" "$GOLDEN"
