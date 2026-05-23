#!/usr/bin/env bash
# ui holomap: capture the rotating planet view + island name strip, byte-
# compare to the committed golden.  Fixture: Anon1 (Citadel Island).
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_holomap.sh
TESTNAME=ui_holomap
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/holomap_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "holomap" "$GOLDEN"
