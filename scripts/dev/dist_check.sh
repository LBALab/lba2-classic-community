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
# Four checks run alongside the captures, about the profiles rather than the
# rendering:
#
#   IDENTITY   per install, the release the engine reports matches the Version
#              its own config declares. Reported as '-' for a disc image, whose
#              config is inside it.
#   WROTE      per install, the folder is unchanged after every run against it.
#              An install is input; a config read out of it must not be written
#              back, which also has to hold on read-only media.
#   ISOLATION  the developer's own user folder is unchanged across the sweep.
#              Every run is given --user-dir, and this is what proves it took.
#   REBIND     a profile seeded from one release, pointed at another, reports
#              the second. Needs two installs declaring different releases.
#
# Exits non-zero if any install fails to boot with its assets, fails to produce
# a capture, or fails one of those four, so it can be used as a check and not
# only read.

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

# The release each run believes it is, read back from the `distrib` command.
distrib_of() { # distrib_of <log>
    grep -oE 'Current: [0-9] \([a-z_]+\)' "$1" | head -1 | sed 's/Current: //'
}

# What an install declares, read straight off its config without asking the
# engine, so IDENTITY has an oracle the engine did not supply. Empty when the
# config is not on the filesystem: a disc image keeps its inside, and reading it
# would mean decoding the image here. Version_US does not match, the colon has
# to follow Version itself.
declared_version() { # declared_version <dir>
    local f
    for f in "$1"/lba2.cfg "$1"/LBA2.CFG "$1"/Lba2.Cfg; do
        [ -f "$f" ] || continue
        grep -aiE '^[[:space:]]*Version[[:space:]]*:' "$f" | head -1 | sed 's/.*://; s/[^0-9]//g'
        return
    done
}

# Where the engine writes when nobody tells it otherwise. Every run below is
# given its own --user-dir, and this is the folder that proves it: a developer's
# real saves and cfg live here, and a sweep that touched them would be doing the
# very thing the profiles exist to prevent.
default_user_dir() {
    case "$(uname -s)" in
        Darwin) printf '%s\n' "$HOME/Library/Application Support/Twinsen/LBA2" ;;
        *)      printf '%s\n' "${XDG_DATA_HOME:-$HOME/.local/share}/Twinsen/LBA2" ;;
    esac
}

# Every path under a tree with its size and mtime, folded to one short hash.
# Stats only, never reads, so it stays cheap over a game folder holding a disc
# image. Python rather than `find -printf`, which is GNU-only.
tree_fingerprint() { # tree_fingerprint <dir>
    python3 - "$1" <<'PY'
import hashlib, os, sys
root = sys.argv[1]
if not os.path.isdir(root):
    print("absent"); raise SystemExit
h = hashlib.sha256()
for dirpath, dirnames, filenames in os.walk(root):
    dirnames.sort(); filenames.sort()
    for name in filenames:
        p = os.path.join(dirpath, name)
        try:
            st = os.stat(p)
        except OSError:
            continue
        h.update(f"{os.path.relpath(p, root)}\0{st.st_size}\0{st.st_mtime_ns}\0".encode())
print(h.hexdigest()[:12])
PY
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

# ISOLATION, opened here and closed after the loop.
USER_DIR_REAL="$(default_user_dir)"
user_dir_before="$(tree_fingerprint "$USER_DIR_REAL")"

# Collected per install for the REBIND check below: name, folder, declared release.
REBIND_ROWS=""

# Captured surfaces: an interior scene (the cold-boot start), an exterior one
# (a different render path, and where the draw-order bugs have historically
# lived), and three UI modals. Each is a chance for a change to show up.
hdr=$(printf '%-13s %-14s %-9s %-9s %-7s %-7s %-7s %-9s %-13s %-13s %-13s %-13s %-13s %-13s' \
      INSTALL DISTRIB IDENTITY LANGUAGE DISC ASSETS WROTE MUSIC INTERIOR EXTERIOR MENU OPTIONS INVENTORY DEMO)
echo "$hdr" | tee -a "$SUMMARY"
printf '%.0s-' {1..168} | tee -a "$SUMMARY"; echo | tee -a "$SUMMARY"

while IFS= read -r entry; do
    [ -n "$entry" ] || continue
    name="${entry%%:*}"; dir="${entry#*:}"
    if [ ! -d "$dir" ]; then
        printf '%-13s %s\n' "$name" "(no such directory: $dir)"
        continue
    fi

    prof="$OUT/profile-$name"; rm -rf "$prof"; mkdir -p "$prof"
    log="$OUT/$name.log"

    # NOPOLLUTE: the install is input, never output. The engine reads a config
    # from here to seed a fresh profile and must not write one back, which also
    # has to hold when the folder is a mounted image or sits on read-only media.
    game_before="$(tree_fingerprint "$dir")"

    # One boot: identity, mount, preflight, and the music decisions.
    run "$prof" "$dir" \
        --exec "audio global log 1; distrib; disc" \
        --exec-at 3 "playmusic 1" --exec-at 6 "playmusic 5" --exec-at 9 "playmusic 6" \
        --tick 12 --exit > "$log"

    distrib=$(distrib_of "$log")
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

    # Every run against this install is done; the folder must look untouched.
    if [ "$(tree_fingerprint "$dir")" = "$game_before" ]; then wrote=ok; else wrote=DIRTY; fi

    # IDENTITY: the fresh profile inherited what the install actually declares.
    want_ver="$(declared_version "$dir")"
    got_ver="${distrib%% *}"
    if [ -z "$want_ver" ]; then identity="-"
    elif [ "$want_ver" = "$got_ver" ]; then identity=ok
    else identity=MISMATCH; fi

    REBIND_ROWS="$REBIND_ROWS$name|$dir|${distrib:-?}"$'\n'

    row=$(printf '%-13s %-14s %-9s %-9s %-7s %-7s %-7s %-9s %-13s %-13s %-13s %-13s %-13s %-13s' \
        "$name" "${distrib:-?}" "$identity" "${lang:-?}" "$disc" "$assets" "$wrote" "$music" \
        "$interior" "$exterior" "$menu_main" "$menu_opts" "$inventory" "$demo")
    echo "$row" | tee -a "$SUMMARY"

    # A verdict, so this can gate something rather than only be read.
    [ "$assets" = ok ] || { echo "  FAIL $name: assets not all present" >&2; failures=$((failures+1)); }
    [ "$wrote" = ok ] || { echo "  FAIL $name: the sweep wrote into the install at $dir" >&2; failures=$((failures+1)); }
    [ "$identity" != MISMATCH ] || { echo "  FAIL $name: install declares Version $want_ver, engine read $got_ver" >&2; failures=$((failures+1)); }
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

echo | tee -a "$SUMMARY"

# --- ISOLATION: the sweep ran entirely inside its own profiles -----------------
# Every run was given --user-dir, so the folder holding the developer's own
# saves, cfg and log must be exactly as it was. Absent on a machine that has
# never run the game, which is not a failure, just nothing to compare.
if [ "$user_dir_before" = absent ]; then
    echo "ISOLATION  skipped (no user folder at $USER_DIR_REAL)" | tee -a "$SUMMARY"
elif [ "$(tree_fingerprint "$USER_DIR_REAL")" = "$user_dir_before" ]; then
    echo "ISOLATION  ok       ($USER_DIR_REAL untouched)" | tee -a "$SUMMARY"
else
    echo "ISOLATION  FAILED   ($USER_DIR_REAL changed during the sweep)" | tee -a "$SUMMARY"
    failures=$((failures+1))
fi

# --- REBIND: a profile follows the install it is pointed at --------------------
# A fresh profile takes its config from the install it first boots against, and
# the release identity comes with it. Point that same profile at a different
# release and the identity has to follow, or the run draws one publisher's logo
# over another's data and looks for voices in a folder this disc does not have.
#
# Needs two installs declaring different values. With fewer there is nothing to
# compare, which is reported rather than passed silently.
rebind_a=""; rebind_b=""
while IFS= read -r r; do
    [ -n "$r" ] || continue
    case "${r##*|}" in ?|'') continue ;; esac   # no identity read, nothing to bind
    if [ -z "$rebind_a" ]; then rebind_a="$r"
    elif [ "${r##*|}" != "${rebind_a##*|}" ]; then rebind_b="$r"; break
    fi
done <<< "$REBIND_ROWS"

if [ -z "$rebind_b" ]; then
    echo "REBIND     skipped  (needs two installs declaring different releases)" | tee -a "$SUMMARY"
else
    a_dir="${rebind_a#*|}"; a_dir="${a_dir%|*}"; a_want="${rebind_a##*|}"
    b_dir="${rebind_b#*|}"; b_dir="${b_dir%|*}"; b_want="${rebind_b##*|}"
    rlog="$OUT/rebind.log"
    rprof="$OUT/profile-rebind"; rm -rf "$rprof"

    run "$rprof" "$a_dir" --exec "distrib" --tick 2 --exit > "$rlog"
    got_a="$(distrib_of "$rlog")"
    run "$rprof" "$b_dir" --exec "distrib" --tick 2 --exit > "$rlog.b"
    got_b="$(distrib_of "$rlog.b")"

    if [ "$got_a" != "$a_want" ]; then
        echo "REBIND     skipped  (seed run reported '$got_a', expected '$a_want')" | tee -a "$SUMMARY"
    elif [ "$got_b" = "$b_want" ]; then
        echo "REBIND     ok       (profile seeded '$a_want' reports '$got_b' against the other install)" | tee -a "$SUMMARY"
    else
        echo "REBIND     FAILED   (profile seeded '$a_want' still reports '$got_b' against an install declaring '$b_want')" | tee -a "$SUMMARY"
        failures=$((failures+1))
    fi
fi

echo
echo "artefacts in $OUT (logs, PNGs, summary.txt)"
echo "compare two runs with: diff <a>/summary.txt <b>/summary.txt"
echo "music key: c=CD audio in image, x=cue track, i=file in image, o=ogg, f=wav, -=not found"
echo "hashes are a per-install baseline: re-run and diff. Do not compare them"
echo "across installs, which legitimately differ in language and logo sprite."
echo "UI captures use --fixed-dt, without which the animated plasma makes them"
echo "hash differently every run."

[ "$failures" -eq 0 ] || { echo "$failures check(s) failed" >&2; exit 1; }
