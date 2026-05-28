#!/usr/bin/env bash
# ui inventory @ 768x480: re-renders the inventory wheel at the Steam Deck
# render width and asserts the centred 640x480 crop of the capture is
# byte-identical to the existing 640 golden. Catches centring / stride /
# anchor regressions at the wider width without needing a per-resolution
# golden.
#
# Pairs with test_ui_inventory.sh (640 pinned). Run both: the 640 test
# protects the canonical render; this one protects the centring invariant
# that the X-axis C2 campaign established for widescreen. Relies on the
# 640 golden being cleanroom (--black-bg, scene-free); strict byte-
# identical wouldn't survive scene bleed at the wider FoV.
TESTNAME=ui_inventory_wide
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
GOLDEN="$REPO/tests/savegame/corpus/baselines/ui/inventory_Anon1.png"

[ -f "$LBA2_TEST_SAVE" ] || skip "fixture save missing: $LBA2_TEST_SAVE"

ui_compare_wide "768x480" "--black-bg inventory" "$GOLDEN"
