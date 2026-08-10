#!/usr/bin/env bash
#
# Regression sweep across the retail distributions we support.
#
# The releases are not interchangeable. Each states its own `Version` in the
# lba2.cfg sitting in its install folder, a fresh profile inherits it, and that
# value picks the music track table, the distributor splash and two logo
# sprites. So a change can be fine on one distribution and wrong on another,
# and the only way to know is to run all of them.
#
# Each install gets a throwaway profile, so what is measured is the new-user
# path (seeded from the install) rather than whatever the developer's own
# profile happens to say.
#
#   scripts/dev/dist_check.sh [outdir]
#
# Renders are hashed, not compared across installs: US and EU legitimately draw
# different sprites, so cross-install equality would be the wrong assertion.
# The hashes are a per-install baseline to diff against a later run.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXE="${LBA2CC:-$ROOT/build/SOURCES/lba2cc}"
OUT="${1:-${TMPDIR:-/tmp}/lba2-dist-check}"

[ -x "$EXE" ] || { echo "no engine at $EXE (set LBA2CC=)" >&2; exit 1; }
mkdir -p "$OUT"

# name:path — edit for your machine, or set LBA2_DIST_LIST to override.
DEFAULT_LIST="steam:$ROOT/../LBA2/Common
gog:$ROOT/../LBA2-GOG
twinsen-rip:$ROOT/../TWINSEN"
LIST="${LBA2_DIST_LIST:-$DEFAULT_LIST}"

run() { # run <profile> <gamedir> <extra args...>
    local prof="$1" dir="$2"; shift 2
    XDG_DATA_HOME="$prof" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout 300 "$EXE" --game-dir "$dir" --no-autosave "$@" 2>&1
}

# The menus animate a plasma strip that differs between two runs of the same
# build, so a whole-image hash changes every time and means nothing. Skip that
# band, as a fraction of height so it survives a resolution change.
PLASMA_FROM=0.34
PLASMA_TO=0.48
hash_png() { # hash_png <png> [mask]
    [ -f "$1" ] || { echo "-"; return; }
    if [ "${2:-}" = mask ]; then
        python3 "$ROOT/scripts/dev/png_hash.py" "$1" "$PLASMA_FROM" "$PLASMA_TO" 2>/dev/null
    else
        python3 "$ROOT/scripts/dev/png_hash.py" "$1" 2>/dev/null
    fi
}

printf '%-13s %-14s %-9s %-7s %-7s %-14s %-13s %-13s\n' \
       INSTALL DISTRIB LANGUAGE DISC ASSETS MUSIC RENDER MENU
printf '%.0s-' {1..99}; echo

while IFS= read -r entry; do
    [ -n "$entry" ] || continue
    name="${entry%%:*}"; dir="${entry#*:}"
    if [ ! -d "$dir" ]; then
        printf '%-13s %s\n' "$name" "(no such directory: $dir)"
        continue
    fi

    prof="$OUT/profile-$name"; rm -rf "$prof"; mkdir -p "$prof"
    log="$OUT/$name.log"

    # One boot: identity, mount, preflight, and the music decisions.
    run "$prof" "$dir" \
        --exec "audio global log 1; distrib; disc" \
        --exec-at 3 "playmusic 1" --exec-at 6 "playmusic 5" --exec-at 9 "playmusic 6" \
        --tick 12 --exit > "$log"

    distrib=$(grep -oE 'Current: [0-9] \([a-z_]+\)' "$log" | head -1 | sed 's/Current: //')
    disc=$(grep -q '^Disc:' "$log" && echo mounted || echo none)
    assets=$(grep -q 'Assets *all present' "$log" && echo ok || echo MISSING)
    # Reported because it explains most cross-install render differences: the
    # menus carry text, and each install's own cfg picks the language.
    lang=$(grep -oE 'Language +[^ ]+' "$log" | head -1 | awk '{print $2}')

    # How each of the three music requests was served. One letter per request:
    # f=filesystem wav, o=ogg, i=in-image file, c=in-image CD audio, x=cue track,
    # -=not found. Distributions legitimately differ here; a change of letter
    # between runs of the same install is the regression.
    music=""
    for want in TADPCM2 JADPCM01 TADPCM6; do
        line=$(grep -E "PlayStream start .*${want}\.WAV" "$log" | head -1)
        case "$line" in
            *"CD-DA track"*)     music+="c" ;;
            *"cue track"*)       music+="x" ;;
            *"cue-audio"*)       music+="x" ;;
            *"WAV (disc)"*)      music+="i" ;;
            *"start OGG"*)       music+="o" ;;
            *"start WAV"*)       music+="f" ;;
            *)                   music+="-" ;;
        esac
    done
    # Spell it out rather than leaving three letters to decode.
    music="$music ($(grep -c 'PlayStream start' "$log"))"

    # A settled gameplay frame, and the main menu, which is one of the surfaces
    # whose sprite depends on the distribution.
    run "$prof" "$dir" --headless --fixed-dt 16 --tick 60 --exit \
        --screenshot "$OUT/$name-scene.png" >> "$log"
    run "$prof" "$dir" --headless --exec "ui menu-main $OUT/$name-menu.png" \
        --tick 4 --exit >> "$log"

    printf '%-13s %-14s %-9s %-7s %-7s %-14s %-13s %-13s\n' \
        "$name" "${distrib:-?}" "${lang:-?}" "$disc" "$assets" "$music" \
        "$(hash_png "$OUT/$name-scene.png")" "$(hash_png "$OUT/$name-menu.png" mask)"
done <<< "$LIST"

echo
echo "artefacts in $OUT (logs, scene and menu PNGs)"
echo "music key: c=CD audio in image, x=cue track, i=file in image, o=ogg, f=wav, -=not found"
echo "hashes are a per-install baseline: re-run and diff. Do not compare them"
echo "across installs, which legitimately differ in language and logo sprite."
