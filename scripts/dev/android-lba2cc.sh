#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Run the engine on an attached ARM device, as if it were the local binary.
#
# Point LBA2_BIN at this and tests/automation runs against the device with no
# change to lib.sh and no change to any fixture:
#
#   export LBA2_BIN=$PWD/scripts/dev/android-lba2cc.sh
#   export LBA2_ANDROID_ENGINE=$PWD/out/build/android_arm64_cli/SOURCES/lba2cc
#   bash tests/automation/test_negative.sh
#
# lib.sh's precheck only requires LBA2_BIN to be an executable file, so this is
# the one seam that also catches the six fixtures that bypass the ctl helpers
# and invoke "$LBA2_BIN" directly.
#
# HOW PATHS CROSS. Fixtures pass HOST paths on argv and then read the results
# back locally. Rather than teach this script which arguments are paths, map
# each host root to a device folder and rewrite every occurrence of it:
#
#   temp roots (TMPDIR, the macOS user temp folder, /tmp)
#                         -> <device>/tmp/<run>/<n>   named paths copied over and back
#   $LBA2_GAME_DIR        -> <device>/gameroot/<same name>   copied once
#   its parent            -> <device>/gameroot    (fixtures pass the install root)
#   save fixture's folder -> <device>/save        copied once
#   <repo root>           -> <device>/repo        named files copied
#
# Substring substitution, longest root first, which is why it also reaches
# paths buried inside an --exec string. A table of "which flag takes a path"
# could not: UI fixtures put their output path inside --exec, and --load and
# --record accept a bare NAME resolved in the user directory. The folder of a
# named temp path is copied back, so .rec siblings, adeline.log and lba2.cfg
# come back without knowing they exist. What the engine prints is mapped back
# to host paths.
#
# Not translated: a path the engine writes INTO a file (a profile's bound game
# dir, for one) keeps its device spelling.
#
# Env:
#   LBA2_ANDROID_ENGINE  lba2cc built with -DLBA2_ANDROID_EXECUTABLE=ON on the
#                        null platform (docs/ANDROID.md)
#   LBA2_ANDROID_DRYRUN  print the translated invocation and exit, touching no
#                        device. The translation is the part worth testing on
#                        its own, so it is testable on its own.
# ---------------------------------------------------------------------------
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/dev/android-lib.sh
. "$HERE/android-lib.sh"

REPO_ROOT="$(cd "$HERE/../.." && pwd)"
ENGINE="${LBA2_ANDROID_ENGINE:-$REPO_ROOT/out/build/android_arm64_cli/SOURCES/lba2cc}"
DRYRUN="${LBA2_ANDROID_DRYRUN:-}"

# One temp and working folder per engine run, so runs in parallel (run.sh -j)
# cannot meet on the device.
RUN_ID="$$.$RANDOM"
DEV_TMP="$ANDROID_DEVICE_DIR/tmp/$RUN_ID"
DEV_REPO="$ANDROID_DEVICE_DIR/repo"
DEV_SAVE="$ANDROID_DEVICE_DIR/save"

# Shared by every run on this machine, whatever TMPDIR each fixture has.
STATE_DIR="${LBA2_ANDROID_STATE:-/tmp/lba2cc-android-state}"
mkdir -p "$STATE_DIR" 2>/dev/null

# The host roots. Resolve to physical paths: a fixture may hand us a path under
# a symlinked TMPDIR, and a prefix that does not match textually silently fails
# to translate, leaving a host path on the device where it cannot resolve.
host_root() { [[ -n "${1:-}" && -e "$1" ]] && (cd "$1" && pwd -P); }

# Collapse "//" and "/seg/../" textually before matching. run.sh's TMPDIR has a
# doubled slash, and lib.sh finds the save fixture at $LBA2_GAME_DIR/../SAVE, so
# an unnormalised argument would translate to <device>/game/../SAVE, OUTSIDE
# the mirror. Purely textual, so it works on a path inside an --exec string as
# readily as on a bare argument.
normalize_path_text() {
    local a prev=""
    a="$(printf '%s' "$1" | sed -E 's#//+#/#g')"
    while [[ "$a" != "$prev" ]]; do
        prev="$a"
        a="$(printf '%s' "$a" | sed -E 's#/[^/ ]+/\.\./#/#g')"
    done
    printf '%s' "$a"
}

# Temp roots. TMPDIR, and the places a fixture's temp files land whatever
# TMPDIR says: on macOS `mktemp -t`, which lib.sh uses for each fixture's user
# dir, writes to the user temp folder, and scripts name /tmp directly. Each gets
# its own folder under DEV_TMP. Kept longest first, so a path under run.sh's
# per-fixture TMPDIR, which sits inside the macOS user temp folder, is matched
# to the TMPDIR root.
declare -a HOST_TMPS=() TMP_GIVEN=()
add_tmp_root() {
    local r x
    r="$(host_root "$1")"
    [[ -n "$r" ]] || return 0
    for x in "${HOST_TMPS[@]+"${HOST_TMPS[@]}"}"; do
        [[ "$x" == "$r" ]] && return 0
    done
    HOST_TMPS+=("$r")
    TMP_GIVEN+=("$(normalize_path_text "${1%/}")")
}
add_tmp_root "${TMPDIR:-/tmp}"
add_tmp_root "$(getconf DARWIN_USER_TEMP_DIR 2>/dev/null)"
add_tmp_root /tmp
TMP_ORDER=()
mapfile -t TMP_ORDER < <(for i in "${!HOST_TMPS[@]}"; do
    printf '%s %s\n' "${#HOST_TMPS[$i]}" "$i"
done | sort -rn | cut -d' ' -f2)

HOST_GAME="$(host_root "${LBA2_GAME_DIR:-}")"

# The data dir keeps its own name under a mirrored parent. Fixtures pass the
# install root (the data dir's parent) and expect the engine to find Common in
# it, so the parent has to translate too. Only the data dir itself is copied.
HOST_GAME_PARENT=""
DEV_GAME_PARENT="$ANDROID_DEVICE_DIR/gameroot"
DEV_GAME="$DEV_GAME_PARENT/game"
if [[ -n "$HOST_GAME" ]]; then
    HOST_GAME_PARENT="$(dirname "$HOST_GAME")"
    DEV_GAME="$DEV_GAME_PARENT/$(basename "$HOST_GAME")"
fi
HOST_REPO="$REPO_ROOT"

# The save fixture's folder is a root of its own: lib.sh finds it at
# $LBA2_GAME_DIR/../SAVE, a sibling of the data directory.
HOST_SAVE=""
if [[ -n "${LBA2_TEST_SAVE:-}" ]]; then
    HOST_SAVE="$(host_root "$(dirname "$LBA2_TEST_SAVE")")"
elif [[ -n "$HOST_GAME" ]]; then
    HOST_SAVE="$(host_root "$HOST_GAME/../SAVE")"
fi

# Substring replacement, not prefix matching, and every occurrence rather than
# the first. An --exec value is a whole console command line with the path in
# the middle of it ("ui inventory /tmp/xxx/shot.png"), so an argument that only
# CONTAINS a host root still has to be rewritten.
#
# Longest host root first: with a game dir nested inside the repo, rewriting the
# repo root first would leave the remainder pointing into the wrong tree.
declare -a ROOT_PAIRS=()
build_root_pairs() {
    local pair i
    local -a pairs=("$HOST_GAME|$DEV_GAME" "$HOST_REPO|$DEV_REPO" "$HOST_SAVE|$DEV_SAVE"
        "$HOST_GAME_PARENT|$DEV_GAME_PARENT")
    for i in "${!HOST_TMPS[@]}"; do
        pairs+=("${HOST_TMPS[$i]}|$DEV_TMP/$i")
    done
    for pair in "${pairs[@]}"; do
        [[ -z "${pair%%|*}" ]] && continue
        ROOT_PAIRS+=("${#pair} $pair")
    done
    mapfile -t ROOT_PAIRS < <(printf '%s\n' "${ROOT_PAIRS[@]}" | sort -rn | cut -d' ' -f2-)
}
build_root_pairs

# A root can reach us under two spellings: as the caller wrote it and as it
# resolves. macOS's default TMPDIR is /var/folders/..., which is a symlink to
# /private/var/folders/..., and mktemp hands out the first while the roots above
# are the second. Rewrite the written spelling to the resolved one, but only at
# the start of a path, or /private/var/... would itself be rewritten.
declare -a ROOT_ALIASES=()
add_alias() {
    local given resolved="$2"
    given="$(normalize_path_text "${1%/}")"
    [[ -n "$given" && -n "$resolved" && "$given" == /* && "$given" != "$resolved" ]] &&
        ROOT_ALIASES+=("$given|$resolved")
}
for i in "${!HOST_TMPS[@]}"; do
    add_alias "${TMP_GIVEN[$i]}" "${HOST_TMPS[$i]}"
done
add_alias "${LBA2_GAME_DIR:-}" "$HOST_GAME"
[[ -n "${LBA2_TEST_SAVE:-}" ]] && add_alias "$(dirname "$LBA2_TEST_SAVE")" "$HOST_SAVE"

# A whole argument that is a relative path to something on this machine is made
# absolute first: the engine runs in another folder on the device. Fixtures
# build such paths from $(dirname "$0").
abs_arg() {
    local a="$1"
    if [[ "$a" != /* && "$a" != -* && -e "$a" ]]; then
        a="$(cd "$(dirname "$a")" && pwd -P)/$(basename "$a")"
    fi
    printf '%s' "$a"
}

canon_arg() {
    local a pair
    a="$(normalize_path_text "$(abs_arg "$1")")"
    for pair in "${ROOT_ALIASES[@]+"${ROOT_ALIASES[@]}"}"; do
        a="$(GIVEN="${pair%%|*}" RESOLVED="${pair##*|}" perl -pe \
            's{(^|[\s=,"])\Q$ENV{GIVEN}\E(?=/|$|[\s,"])}{$1$ENV{RESOLVED}}g' <<<"$a")"
    done
    printf '%s' "$a"
}

translate_arg() {
    local a pair host dev
    a="$(canon_arg "$1")"
    for pair in "${ROOT_PAIRS[@]}"; do
        host="${pair%%|*}"
        dev="${pair##*|}"
        a="${a//"$host"/"$dev"}"
    done
    printf '%s' "$a"
}

TRANSLATED=()
for arg in "$@"; do
    TRANSLATED+=("$(translate_arg "$arg")")
done

# The user dir and game dir travel as environment, not as --user-dir/--game-dir.
# Both reach the same directory, but the engine reports WHERE the setting came
# from, and test_user_dirs.sh and test_cli_override_persistence.sh assert on
# that. Promoting an env var to a flag would change the reported source and fail
# a fixture for a reason that has nothing to do with the device.
DEV_USER_DIR="$(translate_arg "${LBA2_USER_DIR:-${HOST_TMPS[0]:-/tmp}/user}")"

quoted=""
for a in "${TRANSLATED[@]}"; do
    quoted+=" $(printf '%q' "$a")"
done

if [[ -n "$DRYRUN" ]]; then
    echo "engine:   $ANDROID_DEVICE_DIR/bin/lba2cc"
    echo "user-dir: $DEV_USER_DIR"
    echo "game-dir: $DEV_GAME"
    echo "argv:    $quoted"
    exit 0
fi

if [[ ! -x "$ENGINE" ]]; then
    echo "android-lba2cc: no engine at $ENGINE" >&2
    echo "  build one with -DLBA2_ANDROID_EXECUTABLE=ON -DPLATFORM_BACKEND=null (docs/ANDROID.md), or set LBA2_ANDROID_ENGINE" >&2
    exit 1
fi

LBA2_ANDROID_STATE="$STATE_DIR" android_probe_cached || exit 1

# A directory tree is copied once and then reused. The marker is named after a
# hash of every file's path, size and modification time, so a changed tree
# misses it and is copied again from scratch.
if stat --version >/dev/null 2>&1; then
    STAT_FMT=(-c '%n %s %Y')
else
    STAT_FMT=(-f '%N %z %m')
fi
tree_hash() {
    (cd "$1" && find . -type f -exec stat "${STAT_FMT[@]}" {} +) | LC_ALL=C sort | sha256sum | cut -c1-16
}
push_tree_cached() {
    local src="$1" dest="$2" hash
    [[ -d "$src" ]] || return 0
    hash="$(tree_hash "$src")"
    exists_on_device "$dest/.tree-$hash" && return 0
    "$ADB" shell "rm -rf '$dest' && mkdir -p '$dest'" >/dev/null 2>&1
    if ! "$ADB" push "$(host_to_adb_path "$src")/." "$dest/" >/dev/null 2>&1; then
        echo "android-lba2cc: push failed: $src" >&2
        return 1
    fi
    "$ADB" shell "touch '$dest/.tree-$hash'" >/dev/null 2>&1
}

# Parallel runs share the engine, its libraries and the copied trees, so only
# one copies at a time; the others then find the marker and skip. mkdir is the lock because it is atomic
# and needs nothing installed.
LOCK="$STATE_DIR/copy.lock"
lock_wait=0
until mkdir "$LOCK" 2>/dev/null; do
    sleep 1
    lock_wait=$((lock_wait + 1))
    if [[ $lock_wait -ge 900 ]]; then
        echo "android-lba2cc: $LOCK held for 15 minutes; remove it if no run is copying" >&2
        exit 1
    fi
done
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
android_stage_runtime "$ENGINE" || exit 1
android_push_cached "$ENGINE" "$ANDROID_DEVICE_DIR/bin" 755 || exit 1
push_tree_cached "$HOST_GAME" "$DEV_GAME" || exit 1
push_tree_cached "$HOST_SAVE" "$DEV_SAVE" || exit 1

# Files under the repo and the temp roots travel only when an argument names
# them, and only what is named: a fixture's work folder can hold every
# recording and screenshot of its earlier runs. Paths with spaces are not split
# out of a longer argument; no fixture passes one there.
declare -a TMP_ENTRIES=() REPO_FILES=()
collect_token() {
    local tok="$1" i rel
    for i in "${TMP_ORDER[@]}"; do
        if [[ "$tok" == "${HOST_TMPS[$i]}"/* ]]; then
            rel="${tok#"${HOST_TMPS[$i]}"/}"
            TMP_ENTRIES+=("$i|$rel")
            return
        fi
    done
    if [[ "$tok" == "$HOST_REPO"/* && -e "$tok" ]]; then
        REPO_FILES+=("${tok#"$HOST_REPO"/}")
    fi
}
collect_paths() {
    local a="$1" tok
    # A whole argument that names an existing path goes over as it is, spaces
    # and all.
    if [[ "$a" == /* && -e "$a" ]]; then
        collect_token "$a"
        return
    fi
    for tok in ${a//[=,]/ }; do
        tok="${tok#\"}"
        collect_token "${tok%\"}"
    done
}
for arg in "$@"; do
    collect_paths "$(canon_arg "$arg")"
done
[[ -n "${LBA2_USER_DIR:-}" ]] && collect_paths "$(canon_arg "$LBA2_USER_DIR")"
mapfile -t TMP_ENTRIES < <(printf '%s\n' "${TMP_ENTRIES[@]+"${TMP_ENTRIES[@]}"}" | LC_ALL=C sort -u | sed '/^$/d')

# A named temp path that exists goes over (SEND). The folders the engine may
# write in are watched and copied back (WATCH): a named folder itself, or the
# folder holding a named file or a file still to be written, which is where
# its siblings land (a recording's .rec beside it, a screenshot). On the device
# such a folder holds only what was sent and what the engine wrote.
declare -a SEND=() WATCH=()
for entry in "${TMP_ENTRIES[@]+"${TMP_ENTRIES[@]}"}"; do
    i="${entry%%|*}"
    rel="${entry#*|}"
    host="${HOST_TMPS[$i]}/$rel"
    if [[ -d "$host" && ! -L "$host" ]]; then
        SEND+=("$entry")
        WATCH+=("$entry")
        continue
    fi
    [[ -e "$host" || -L "$host" ]] && SEND+=("$entry")
    parent="$(dirname "$rel")"
    [[ "$parent" == "." ]] && parent=""
    WATCH+=("$i|$parent")
done
# Drop an entry inside another of the same list: it travels with it.
outermost() {
    local e o keep
    for e in "$@"; do
        keep=1
        for o in "$@"; do
            [[ "$o" == "$e" || "${o%%|*}" != "${e%%|*}" ]] && continue
            if [[ "${o#*|}" == "" || "${e#*|}" == "${o#*|}/"* ]]; then
                keep=0
                break
            fi
        done
        [[ $keep == 1 ]] && printf '%s\n' "$e"
    done | LC_ALL=C sort -u
}
mapfile -t SEND < <(outermost "${SEND[@]+"${SEND[@]}"}")
mapfile -t WATCH < <(outermost "${WATCH[@]+"${WATCH[@]}"}")

for rel in "${REPO_FILES[@]+"${REPO_FILES[@]}"}"; do
    "$ADB" shell "mkdir -p '$DEV_REPO/$(dirname "$rel")'" >/dev/null 2>&1
    if [[ -d "$HOST_REPO/$rel" ]]; then
        push_tree_cached "$HOST_REPO/$rel" "$DEV_REPO/$rel" || exit 1
    else
        android_push_cached "$HOST_REPO/$rel" "$DEV_REPO/$(dirname "$rel")" 644 || exit 1
    fi
done

rmdir "$LOCK" 2>/dev/null
trap - EXIT

# Device folders left by runs that were killed before they could clean up.
# Each is named after the PID of the run that made it, on this machine.
stale=""
while IFS= read -r d; do
    d="${d%$'\r'}"
    [[ "$d" =~ ^(tmp|cwd)/([0-9]+)\.[0-9]+$ ]] || continue
    kill -0 "${BASH_REMATCH[2]}" 2>/dev/null || stale+=" '$ANDROID_DEVICE_DIR/$d'"
done < <("$ADB" shell "cd '$ANDROID_DEVICE_DIR' 2>/dev/null && for d in tmp/* cwd/*; do [ -d \"\$d\" ] && echo \"\$d\"; done" 2>/dev/null)
[[ -z "$stale" ]] || "$ADB" shell "rm -rf$stale" >/dev/null 2>&1

# Refuse to send a named folder that is not what a fixture makes: a path under
# /tmp that names a large folder would otherwise go over in full.
TMP_ENTRY_LIMIT_KB=$((512 * 1024))
mkdirs=""
for i in "${!HOST_TMPS[@]}"; do
    mkdirs+=" $(printf '%q' "$DEV_TMP/$i")"
done
# Only folders that exist here: the engine must fail to write into a missing
# one on the device just as it would on this machine.
for entry in "${WATCH[@]+"${WATCH[@]}"}"; do
    [[ -d "${HOST_TMPS[${entry%%|*}]}/${entry#*|}" ]] || continue
    mkdirs+=" $(printf '%q' "$DEV_TMP/${entry%%|*}/${entry#*|}")"
done
"$ADB" shell "rm -rf '$DEV_TMP' && mkdir -p$mkdirs" >/dev/null 2>&1

# What each copied file held, by "<root index>|<path under the root>". Copying
# back compares with it, so only what the engine wrote comes back: the fixture
# may be writing into the same folder meanwhile (its own stdout redirect, for
# one), and its files must not be overwritten or removed.
declare -A PUSHED=()
file_hash() { sha256sum "$1" | cut -c1-32; }

# Temp entries cross as tar streams, one round trip each way. tar keeps empty
# folders, which adb push drops (a fresh mktemp -d output folder is one), and
# keeps symlinks as symlinks. A symlink is then pointed at the device copy of
# its target: fixtures build a fake install out of links to the game data.
for entry in "${SEND[@]+"${SEND[@]}"}"; do
    i="${entry%%|*}"
    rel="${entry#*|}"
    root="${HOST_TMPS[$i]}"
    [[ -e "$root/$rel" || -L "$root/$rel" ]] || continue
    if [[ "$(du -sk "$root/$rel" | cut -f1)" -gt $TMP_ENTRY_LIMIT_KB ]]; then
        echo "android-lba2cc: $root/$rel is over 512 MB; not copying a whole temp entry that size" >&2
        exit 1
    fi
    while IFS= read -r -d '' f; do
        PUSHED["$i|${f#"$root"/}"]="$(file_hash "$f")"
    done < <(find "$root/$rel" -type f -print0)
    if ! tar -cf - -C "$root" "$rel" | "$ADB" shell "tar -xf - -C '$DEV_TMP/$i'" >/dev/null 2>&1; then
        echo "android-lba2cc: copy to the device failed: $root/$rel" >&2
        exit 1
    fi
    relinks=""
    while IFS= read -r -d '' link; do
        target="$(perl -MCwd=abs_path -e 'print abs_path(shift) // ""' "$link")"
        [[ -n "$target" ]] || continue
        dev_target="$(translate_arg "$target")"
        [[ "$dev_target" == "$ANDROID_DEVICE_DIR"/* ]] || continue
        relinks+="ln -sfn $(printf '%q' "$dev_target") $(printf '%q' "$DEV_TMP/$i/${link#"$root"/}"); "
    done < <(find "$root/$rel" -type l -print0)
    [[ -z "$relinks" ]] || "$ADB" shell "$relinks" >/dev/null 2>&1
done

# Copy the temp entries back. A file comes back when the engine created or
# changed it, judged against what was copied over; symlinks on the device are
# left out, since following one would bring the game data back. With "final",
# a file the engine removed is removed here too, unless the fixture has
# changed it since.
sync_back() {
    local final="${1:-}" entry i rel root staging f key h at
    local -A seen=()
    for entry in "${WATCH[@]+"${WATCH[@]}"}"; do
        i="${entry%%|*}"
        rel="${entry#*|}"
        at="${rel:-.}"
        root="${HOST_TMPS[$i]}"
        staging="$(mktemp -d "$root/.pull-XXXXXX")"
        if ! "$ADB" exec-out "cd '$DEV_TMP/$i' && [ -e '$at' ] && find '$at' ! -type l -print0 | tar --null --no-recursion -T - -cf -" 2>/dev/null |
            tar -xf - -C "$staging" 2>/dev/null; then
            rm -rf "$staging"
            # Nothing to copy when the engine removed the folder; otherwise a
            # live copy can retry, the last one cannot.
            if [[ -n "$final" ]] && exists_on_device "$DEV_TMP/$i/$at"; then
                echo "android-lba2cc: copy back failed: $DEV_TMP/$i/$at" >&2
                return 1
            fi
            continue
        fi
        while IFS= read -r -d '' f; do
            f="${f#"$staging"/}"
            f="${f#./}"
            [[ -z "$f" || "$f" == "." || "$f" == .pull-* ]] && continue
            mkdir -p "$root/$f"
        done < <(find "$staging/$at" -type d -print0 2>/dev/null)
        while IFS= read -r -d '' f; do
            local name="${f#"$staging"/}"
            name="${name#./}"
            key="$i|$name"
            seen["$key"]=1
            h="$(file_hash "$f")"
            [[ "${PUSHED[$key]-}" == "$h" ]] && continue
            mkdir -p "$(dirname "$root/$name")"
            cp -p "$f" "$root/$name"
            PUSHED["$key"]="$h"
        done < <(find "$staging/$at" -type f -print0 2>/dev/null)
        rm -rf "$staging"
        if [[ -n "$final" ]]; then
            for key in "${!PUSHED[@]}"; do
                [[ "${key%%|*}" == "$i" ]] || continue
                [[ -z "$rel" || "${key#*|}" == "$rel/"* ]] || continue
                [[ -n "${seen[$key]-}" ]] && continue
                f="$root/${key#*|}"
                [[ -f "$f" && ! -L "$f" && "$(file_hash "$f")" == "${PUSHED[$key]}" ]] && rm -f "$f"
            done
        fi
    done
}

# The engine reads these from its environment. Paths in them are translated
# like arguments; HOME is pointed inside the device dir so a run with no user
# dir cannot write anywhere else.
env_line="HOME='$ANDROID_DEVICE_DIR/home' LD_LIBRARY_PATH='$ANDROID_DEVICE_DIR/lib'"
for v in LBA2_USER_DIR LBA2_GAME_DIR LBA2_LOG_LEVEL LBA2_PROFILE LBA2_REC_HASH_EVERY \
    LBA2_REC_HASH32 LBA2_SAVE_LOAD_ABI LBA2_TEXFILTER LBA2_PRESENT_FILTER LBA2_ISO_DIV \
    LBA2_DBG_ISOLATE NO_COLOR; do
    [[ -n "${!v+set}" ]] || continue
    env_line+=" $v=$(printf '%q' "$(translate_arg "${!v}")")"
done

# What the engine prints names device paths; fixtures compare them with the
# paths they passed. Map each device root back to the spelling the caller used,
# longest first.
declare -a BACK_PAIRS=()
for i in "${!HOST_TMPS[@]}"; do
    BACK_PAIRS+=("$DEV_TMP/$i|${TMP_GIVEN[$i]}")
done
[[ -n "${LBA2_GAME_DIR:-}" ]] && BACK_PAIRS+=("$DEV_GAME|$(normalize_path_text "${LBA2_GAME_DIR%/}")"
    "$DEV_GAME_PARENT|$(dirname "$(normalize_path_text "${LBA2_GAME_DIR%/}")")")
[[ -n "${LBA2_TEST_SAVE:-}" ]] && BACK_PAIRS+=("$DEV_SAVE|$(dirname "$LBA2_TEST_SAVE")")
BACK_PAIRS+=("$DEV_REPO|$HOST_REPO")
BACK_MAP="$(for pair in "${BACK_PAIRS[@]}"; do printf '%s %s\n' "${#pair}" "$pair"; done |
    sort -rn | cut -d' ' -f2-)"
export BACK_MAP
back_filter() {
    perl -e '
        my @p = map { [split /\|/, $_, 2] } grep { length } split /\n/, $ENV{BACK_MAP};
        $| = 1;
        while (my $l = <STDIN>) { for (@p) { my ($d, $h) = @$_; $l =~ s/\Q$d\E/$h/g } print $l }'
}

# The engine never reads stdin, so adb is given none. Otherwise it would take
# input meant for the fixture, such as the rest of a file a loop is reading.
#
# adb runs in the background while the temp entries are copied back about once
# a second: some fixtures watch a log while the engine runs and then kill it.
# It is a direct child of this script, because that is what such a fixture
# kills (pkill -P), and ending adb ends the engine on the device. A signal we
# can catch is passed on to it the same way.
run_dir="$ANDROID_DEVICE_DIR/cwd/$RUN_ID"
exec 3> >(back_filter >&2)
err_filter=$!
exec 4> >(back_filter)
out_filter=$!
"$ADB" shell "rm -rf '$run_dir' && mkdir -p '$run_dir' '$ANDROID_DEVICE_DIR/home' && cd '$run_dir' && $env_line '$ANDROID_DEVICE_DIR/bin/$(basename "$ENGINE")'$quoted" </dev/null 2>&3 >&4 &
engine_pid=$!
trap 'kill "$engine_pid" 2>/dev/null; wait "$engine_pid" 2>/dev/null; exec 3>&- 4>&-; exit 143' TERM INT HUP
if [[ ${#WATCH[@]} -gt 0 ]]; then
    while kill -0 "$engine_pid" 2>/dev/null; do
        sleep 1
        kill -0 "$engine_pid" 2>/dev/null && sync_back
    done
fi
wait "$engine_pid"
rc=$?
trap - TERM INT HUP
exec 3>&- 4>&-
wait "$err_filter" "$out_filter" 2>/dev/null

sync_back final || rc=1
"$ADB" shell "rm -rf '$DEV_TMP' '$run_dir'" >/dev/null 2>&1

exit "$rc"
