#!/usr/bin/env bash
# ui holoplan: capture the zoomed-in planet/island view (post-globe-select)
# for the Citadel island (0), byte-compare to the committed golden.
# Fixture: Anon1.  Exercises the HOLOPLAN.CPP C2 routing (corner brackets at
# (MDX-1, MDY-1), projection centre at (MDX/2, MDY/2), full-screen dirty box,
# zoom-in animation final coords, ScaleBox source rect).
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_holoplan.sh
TESTNAME=ui_holoplan
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/holoplan_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "holoplan" "0" "$GOLDEN"
