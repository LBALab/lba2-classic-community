#!/usr/bin/env bash
# The presented Options page must replace every pixel of the main menu.
TESTNAME=ui_menu_main_options
. "$(dirname "$0")/lib.sh"
precheck

LBA2_TEST_SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA"
need_save

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

for state in fresh returning; do
    export LBA2_USER_DIR="$tmp/$state"
    seed_menu_save_dir "$LBA2_USER_DIR" "$state" || fail "$state: save setup failed"
    for language in EN SP; do
        for resolution in 640x480 1280x720; do
            out="$tmp/options.png"
            rm -f "$out"
            if ! ctl --language "$language" --resolution "$resolution" \
                --exec "ui menu-main-options $(engine_path "$out")" \
                --fixed-dt 16 --tick 200 --exit >"$tmp/run.log" 2>&1; then
                tail -30 "$tmp/run.log"
                fail "$state/$language/$resolution: engine failed"
            fi
            if grep -Fq -- '[control] --language: invalid' "$tmp/run.log"; then
                fail "$state/$language/$resolution: language override rejected"
            fi
            [ -s "$out" ] || fail "$state/$language/$resolution: capture missing"
            grep -q 'presented_matches_rendered=1' "$tmp/run.log" \
                || fail "$state/$language/$resolution: stale main-menu pixels remain visible"
        done
    done
done

pass "Options replaces the full main menu across save states, languages and resolutions"
