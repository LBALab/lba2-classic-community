#!/usr/bin/env bash
# ui dialog: capture the dialogue bubble + portrait + typewriter text for a
# known text-id, byte-compare to the committed golden.  Fixture: Anon1's
# per-island dialogue bank, text-id 1 ("Il vous faut un bulletin de
# consigne pour escapé.").
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_dialog.sh
TESTNAME=ui_dialog
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/dialog_Anon1_t1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "--black-bg dialog 1" "$GOLDEN"
