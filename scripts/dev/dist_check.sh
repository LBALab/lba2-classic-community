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
# The hashes are a per-install baseline. A summary is written to <outdir>, so
# comparing two runs is `diff a/summary.txt b/summary.txt`.
#
# Exits non-zero if any install fails to boot with its assets, or fails to
# produce a capture, so it can be used as a check and not only read.

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
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout 300 "$EXE" --user-dir "$prof" --game-dir "$dir" --no-autosave "$@" 2>&1
}

hash_png() { # hash_png <png>
    [ -f "$1" ] || { echo "-"; return; }
    python3 "$ROOT/scripts/dev/png_hash.py" "$1" 2>/dev/null
}

# The menus animate a plasma strip on the clock, so a UI capture is only
# reproducible with --fixed-dt. Without it the same screen hashes differently
# every run; with it, no masking is needed at all.
hash_modal() { # hash_modal <profile> <gamedir> <modal> <name>
    local prof="$1" dir="$2" modal="$3" name="$4"
    local png="$OUT/$name-$modal.png"
    run "$prof" "$dir" --headless --fixed-dt 16 --exec "ui $modal $png" --tick 4 --exit >> "$log"
    hash_png "$png"
}

SUMMARY="$OUT/summary.txt"
: > "$SUMMARY"
failures=0

# Captured surfaces: an interior scene (the cold-boot start), an exterior one
# (a different render path, and where the draw-order bugs have historically
# lived), and three UI modals. Each is a chance for a change to show up.
hdr=$(printf '%-13s %-14s %-9s %-7s %-7s %-9s %-13s %-13s %-13s %-13s %-13s %-13s' \
      INSTALL DISTRIB LANGUAGE DISC ASSETS MUSIC INTERIOR EXTERIOR MENU OPTIONS INVENTORY DEMO)
echo "$hdr" | tee -a "$SUMMARY"
printf '%.0s-' {1..150} | tee -a "$SUMMARY"; echo | tee -a "$SUMMARY"

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
            # Matched loosely: a disc whose cue is a real table of contents logs
            # "CD-DA track N", and one that pressed a single theme logs "CD-DA
            # (the disc's only audio track)". Both are audio out of the image.
            *"CD-DA"*)           music+="c" ;;
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

    # Settled frames. --fixed-dt keeps the tick budget from depending on how
    # fast this machine is, which is what makes the hashes reproducible.
    run "$prof" "$dir" --headless --fixed-dt 16 --tick 60 --exit \
        --screenshot "$OUT/$name-interior.png" >> "$log"
    run "$prof" "$dir" --headless --fixed-dt 16 --exec "cube 40" --tick 90 --exit \
        --screenshot "$OUT/$name-exterior.png" >> "$log"
    menu_main=$(hash_modal "$prof" "$dir" menu-main "$name")
    menu_opts=$(hash_modal "$prof" "$dir" menu-options "$name")
    inventory=$(hash_modal "$prof" "$dir" inventory "$name")

    # Demo mode is the one surface where DistribVersion actually shows: the
    # logo it swaps (OBJECT.CPP, "incrust logo demo", top right) is drawn only
    # when DemoSlide is set, which is why every other capture matches across
    # releases.
    run "$prof" "$dir" --headless --fixed-dt 16 --demo --tick 40 --exit \
        --screenshot "$OUT/$name-demo.png" >> "$log"

    interior=$(hash_png "$OUT/$name-interior.png")
    exterior=$(hash_png "$OUT/$name-exterior.png")
    demo=$(hash_png "$OUT/$name-demo.png")

    row=$(printf '%-13s %-14s %-9s %-7s %-7s %-9s %-13s %-13s %-13s %-13s %-13s %-13s' \
        "$name" "${distrib:-?}" "${lang:-?}" "$disc" "$assets" "$music" \
        "$interior" "$exterior" "$menu_main" "$menu_opts" "$inventory" "$demo")
    echo "$row" | tee -a "$SUMMARY"

    # A verdict, so this can gate something rather than only be read.
    [ "$assets" = ok ] || { echo "  FAIL $name: assets not all present" >&2; failures=$((failures+1)); }
    # Test the hashes themselves, not the printed row. A data-only image serves no
    # CD audio, so its music column reads "---", and matching a dash anywhere in
    # the line reported that as a missing screenshot on an install where all six
    # captures were present.
    for h in "$interior" "$exterior" "$menu_main" "$menu_opts" "$inventory" "$demo"; do
        [ "$h" = "-" ] || continue
        echo "  FAIL $name: a capture is missing" >&2
        failures=$((failures+1))
        break
    done
done <<< "$LIST"

echo
echo "artefacts in $OUT (logs, PNGs, summary.txt)"
echo "compare two runs with: diff <a>/summary.txt <b>/summary.txt"
echo "music key: c=CD audio in image, x=cue track, i=file in image, o=ogg, f=wav, -=not found"
echo "hashes are a per-install baseline: re-run and diff. Do not compare them"
echo "across installs, which legitimately differ in language and logo sprite."
echo "UI captures use --fixed-dt, without which the animated plasma makes them"
echo "hash differently every run."

[ "$failures" -eq 0 ] || { echo "$failures install(s) failed" >&2; exit 1; }
