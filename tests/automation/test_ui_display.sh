#!/usr/bin/env bash
# ui display: capture the Display options modal (the Phase 2 menu surface for
# runtime resolution switching) and byte-compare to the committed golden.
#
# Headless captures see only the Classic entry because SDL_VIDEODRIVER=dummy
# returns no display dimensions, so Window_GetDisplayDimensions returns false
# and the Widescreen-for-display entry is suppressed. That's the right
# graceful-degrade behaviour, and the resulting golden is the canonical
# fallback frame — pin it.
#
# --black-bg renders the modal over solid black instead of the live 3D
# scene, matching the cleanroom convention used by inventory / dialog /
# menu-options.
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_display.sh
TESTNAME=ui_display
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/display_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "--black-bg display" "$GOLDEN"
