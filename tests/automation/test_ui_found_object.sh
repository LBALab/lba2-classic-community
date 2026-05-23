#!/usr/bin/env bash
# ui found-object: capture the found-object cinematic (3D rotation of the
# discovered item alongside the typewriter dialogue), byte-compare to the
# committed golden.  Fixture: Anon1 + numvar=0 (Holomap; the cinematic
# shows "Tu viens de retrouver ton Holomap!" with the holomap object).
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_found_object.sh
TESTNAME=ui_found_object
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/found_object_Anon1_n0.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "found-object 0" "$GOLDEN"
