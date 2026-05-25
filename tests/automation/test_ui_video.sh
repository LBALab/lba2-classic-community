#!/usr/bin/env bash
# ui video: capture the first composited frame of the INTRO cinematic, byte-
# compare to the committed golden. Exercises the PLAYACF.CPP letterbox
# centring (Smacker frame written directly into Log at (cineX0, cineY0) with
# proper row stride; pre-cleared sidebars).
#
# Fixture: any save will do — the harness fires PlayAcf directly via the
# console verb, the player's location/state doesn't influence the cinematic
# decode. Anon1 is the standard corpus save.
#
# Regenerate the golden with: LBA2_UI_REGEN=1 bash tests/automation/test_ui_video.sh
TESTNAME=ui_video
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/video_INTRO.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare "video" "INTRO" "$GOLDEN"
