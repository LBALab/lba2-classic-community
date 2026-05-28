#!/usr/bin/env bash
# ui inventory: capture the in-game inventory wheel and byte-compare to the
# committed golden.  Uses Anon1.LBA as the fixture save (early-game state
# with a few items in inventory so the wheel has non-empty slots).
#
# --black-bg renders the wheel over solid black instead of the live 3D
# scene, so the golden is cleanroom UI-only.  See cmd_ui in CONSOLE_CMD.CPP.
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_inventory.sh
TESTNAME=ui_inventory
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/inventory_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "--black-bg inventory" "$GOLDEN"
