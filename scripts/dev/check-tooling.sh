#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# check-tooling.sh — report which external tools this clone can and cannot use
#
# The executable half of docs/TOOLING.md. Tools are grouped by what breaks
# without them, not by how nice they are to have:
#
#   Tier 1  build & run     — no binary without it            (exit 1 if missing)
#   Tier 2  pass review     — CI fails you without it         (warn; --strict fails)
#   Tier 3  per lane        — required only for one task       (report)
#   Tier 4  faster, not required                               (report)
#
# Version floors and pins are READ FROM THE FILE THAT OWNS THEM, never
# duplicated here — so this script cannot drift from the build the way a
# hand-maintained list would. The owner file is named in each row's detail.
#
# Usage:
#   scripts/dev/check-tooling.sh              # all tiers
#   scripts/dev/check-tooling.sh --tier 1     # one tier only (repeatable)
#   scripts/dev/check-tooling.sh --strict     # also exit non-zero on tier-2 gaps
#   scripts/dev/check-tooling.sh --quiet      # only rows that need attention
# ---------------------------------------------------------------------------
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Every version floor below is parsed out of a file under REPO_ROOT. If the
# root is wrong the report degrades to defaults instead of saying so, so fail
# loudly here rather than printing a plausible but unanchored table.
if [ ! -f "$REPO_ROOT/CMakeLists.txt" ]; then
    echo "check-tooling.sh: cannot find the repo root (looked in '$REPO_ROOT')" >&2
    exit 2
fi

# ---- Options --------------------------------------------------------------
STRICT=0
QUIET=0
WANT_TIERS=""

# Print the header block above verbatim, minus the rule lines. Derived from
# the file rather than a line range so editing the header cannot desync --help.
usage() {
    awk 'NR > 1 {
        if ($0 !~ /^#/) exit
        if ($0 ~ /^# *-{10,}$/) next
        sub(/^# ?/, ""); print
    }' "$0"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --tier) WANT_TIERS="${WANT_TIERS}${2} "; shift 2 ;;
        --tier=*) WANT_TIERS="${WANT_TIERS}${1#*=} "; shift ;;
        --strict) STRICT=1; shift ;;
        --quiet|-q) QUIET=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "check-tooling.sh: unknown argument '$1' (try --help)" >&2; exit 2 ;;
    esac
done

want_tier() {
    [ -z "$WANT_TIERS" ] && return 0
    case " $WANT_TIERS " in *" $1 "*) return 0 ;; *) return 1 ;; esac
}

# ---- Output ---------------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_OK=$'\033[32m'; C_WARN=$'\033[33m'; C_BAD=$'\033[31m'
    C_DIM=$'\033[2m'; C_HEAD=$'\033[1m'; C_OFF=$'\033[0m'
else
    C_OK=""; C_WARN=""; C_BAD=""; C_DIM=""; C_HEAD=""; C_OFF=""
fi

TIER1_GAPS=0
TIER2_GAPS=0
CUR_TIER=0

# Headings print lazily, on the first row that survives filtering, so a tier
# the caller filtered out (or that --quiet emptied) leaves no stray title.
PENDING_HEADING=""

heading() {
    CUR_TIER="$1"
    PENDING_HEADING="$(printf '\n%sTier %s — %s%s' "$C_HEAD" "$1" "$2" "$C_OFF")"
}

# row <status> <name> <detail>
#   status: ok | gap | info   (gap = absent or wrong version)
row() {
    local status="$1" name="$2" detail="${3:-}"
    # Only count gaps in tiers the caller asked about, so --tier 1 --strict
    # cannot fail on a tier-2 gap it never showed.
    if [ "$status" = gap ] && want_tier "$CUR_TIER"; then
        case "$CUR_TIER" in
            1) TIER1_GAPS=$((TIER1_GAPS + 1)) ;;
            2) TIER2_GAPS=$((TIER2_GAPS + 1)) ;;
        esac
    fi
    want_tier "$CUR_TIER" || return 0
    [ "$QUIET" = 1 ] && [ "$status" = ok ] && return 0

    if [ -n "$PENDING_HEADING" ]; then
        printf '%s\n' "$PENDING_HEADING"
        PENDING_HEADING=""
    fi

    local label color
    case "$status" in
        ok)   label="ok     "; color="$C_OK" ;;
        gap)  case "$CUR_TIER" in
                  1) label="MISSING"; color="$C_BAD" ;;
                  *) label="absent "; color="$C_WARN" ;;
              esac ;;
        *)    label="--     "; color="$C_DIM" ;;
    esac
    printf '  %s%s%s  %-22s %s%s%s\n' \
        "$color" "$label" "$C_OFF" "$name" "$C_DIM" "$detail" "$C_OFF"
}

have() { command -v "$1" >/dev/null 2>&1; }

# first_of <tool>... — echo the first tool on PATH, empty if none
first_of() {
    local t
    for t in "$@"; do
        if have "$t"; then echo "$t"; return 0; fi
    done
    return 1
}

# ver_ge <have> <want> — semantic-ish compare via sort -V
ver_ge() {
    [ "$1" = "$2" ] && return 0
    [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -1)" = "$2" ]
}

# ---- Pins, read from their owner files ------------------------------------
# Each of these is the single source of truth for that version in the repo.
# If a parse fails we say so rather than silently comparing against nothing.
CMAKE_MIN="$(sed -n 's/^cmake_minimum_required(VERSION \([0-9][0-9.]*\)).*/\1/p' \
    "$REPO_ROOT/CMakeLists.txt" 2>/dev/null | head -1)"

CLANG_FORMAT_MAJOR=""
if [ -r "$REPO_ROOT/scripts/ci/clang-format-select.sh" ]; then
    # shellcheck disable=SC1091
    CLANG_FORMAT_MAJOR="$(sed -n 's/^CLANG_FORMAT_MAJOR=\([0-9]*\).*/\1/p' \
        "$REPO_ROOT/scripts/ci/clang-format-select.sh" | head -1)"
fi

UASM_PIN="$(sed -n 's/^ARG UASM_VERSION=//p' "$REPO_ROOT/docker/Dockerfile.test" 2>/dev/null)"
SDL3_PIN="$(sed -n 's/.*--branch \(release-[0-9][0-9.]*\).*/\1/p' \
    "$REPO_ROOT/docker/Dockerfile.test" 2>/dev/null | head -1)"
NDK_MIN="$(sed -n 's/.*Android NDK \(r[0-9]*+\).*/\1/p' \
    "$REPO_ROOT/scripts/dev/build-android.sh" 2>/dev/null | head -1)"
JDK_VER="$(sed -n "s/^ *java-version: *['\"]\{0,1\}\([0-9][0-9.]*\)['\"]\{0,1\} *$/\1/p" \
    "$REPO_ROOT/.github/workflows/reusable-build-android.yml" 2>/dev/null | head -1)"

# The three linter versions the Lint workflow pins. Read them the same way as
# every other floor so this script tracks a bump without being edited.
lint_pin() {
    sed -n "s/^ *$1: *['\"]\{0,1\}\([0-9][0-9.]*\)['\"]\{0,1\} *$/\1/p" \
        "$REPO_ROOT/.github/workflows/lint.yml" 2>/dev/null | head -1
}
SHELLCHECK_PIN="$(lint_pin SHELLCHECK_VERSION)"
ACTIONLINT_PIN="$(lint_pin ACTIONLINT_VERSION)"
RUFF_PIN="$(lint_pin RUFF_VERSION)"
LYCHEE_PIN="$(sed -n "s/^ *LYCHEE_VERSION: *['\"]\{0,1\}v\{0,1\}\([0-9][0-9.]*\)['\"]\{0,1\} *$/\1/p" \
    "$REPO_ROOT/.github/workflows/docs-links.yml" 2>/dev/null | head -1)"

# pin_note <local> <pinned> — say how the local version relates to CI's pin.
pin_note() {
    if [ -z "$2" ]; then echo "$1"
    elif [ "$1" = "$2" ]; then echo "$1 — matches the CI pin"
    else echo "$1 — CI pins $2"
    fi
}

case "$(uname -s)" in
    Linux)   HOST=linux ;;
    Darwin)  HOST=macos ;;
    MINGW*|MSYS*|CYGWIN*) HOST=windows ;;
    *)       HOST=other ;;
esac

printf '%sTooling check%s  %s(%s, host: %s)%s\n' \
    "$C_HEAD" "$C_OFF" "$C_DIM" "$REPO_ROOT" "$HOST" "$C_OFF"

# ---------------------------------------------------------------------------
# Tier 1 — build & run
# ---------------------------------------------------------------------------
heading 1 "build & run"

# On Windows the whole toolchain hangs off which MSYS2 environment the shell
# came up in. In plain MSYS, cmake/gcc/ninja are simply not on PATH and every
# row below fails with no hint as to why, so name the environment first.
if [ "$HOST" = windows ]; then
    case "${MSYSTEM:-}" in
        UCRT64|MINGW64|MINGW32|CLANG64|CLANGARM64)
            row info "MSYS2 environment" "MSYSTEM=$MSYSTEM" ;;
        "")
            row info "MSYS2 environment" "MSYSTEM unset — expected UCRT64 (docs/WINDOWS.md)" ;;
        *)
            row info "MSYS2 environment" "MSYSTEM=$MSYSTEM is not a toolchain env; use UCRT64 (docs/WINDOWS.md)" ;;
    esac
fi

if have cmake; then
    v="$(cmake --version 2>/dev/null | sed -n '1s/.*version \([0-9][0-9.]*\).*/\1/p')"
    if [ -z "$CMAKE_MIN" ]; then
        row ok "cmake" "$v (could not read the floor from CMakeLists.txt)"
    elif ver_ge "$v" "$CMAKE_MIN"; then
        row ok "cmake" "$v — floor $CMAKE_MIN (CMakeLists.txt)"
    else
        row gap "cmake" "$v is below the $CMAKE_MIN floor (CMakeLists.txt)"
    fi
else
    row gap "cmake" "need ${CMAKE_MIN:-3.23}+ — see docs/TOOLING.md"
fi

if have ninja; then
    row ok "ninja" "$(ninja --version 2>/dev/null)"
else
    row gap "ninja" "every CMakePresets.json preset uses the Ninja generator"
fi

if cxx="$(first_of "${CXX:-}" c++ g++ clang++)"; then
    row ok "C++ compiler" "$cxx $("$cxx" --version 2>/dev/null | sed -n '1s/.*) \{0,1\}\([0-9][0-9.]*\).*/\1/p')"
else
    row gap "C++ compiler" "need a C++98-capable GCC or Clang"
fi

sdl3_ver=""
if have pkg-config && pkg-config --exists sdl3 2>/dev/null; then
    sdl3_ver="$(pkg-config --modversion sdl3 2>/dev/null) (pkg-config)"
elif have cmake; then
    # The build uses find_package(SDL3 CONFIG), so ask CMake the same way
    # rather than guessing at install prefixes.
    probe="$(mktemp -d 2>/dev/null)" || probe=""
    if [ -n "$probe" ]; then
        cat > "$probe/CMakeLists.txt" <<'PROBE'
cmake_minimum_required(VERSION 3.16)
project(sdl3probe NONE)
find_package(SDL3 CONFIG REQUIRED)
message(STATUS "SDL3PROBE=${SDL3_VERSION}")
PROBE
        if out="$(cmake -S "$probe" -B "$probe/b" 2>/dev/null)"; then
            sdl3_ver="$(printf '%s\n' "$out" | sed -n 's/.*SDL3PROBE=\([0-9][0-9.]*\).*/\1/p' | head -1)"
            sdl3_ver="${sdl3_ver:-found} (CMake config)"
        fi
        rm -rf "$probe"
    fi
fi
if [ -n "$sdl3_ver" ]; then
    row ok "SDL3" "$sdl3_ver"
else
    row gap "SDL3" "shared library + CMake config; release builds pin ${SDL3_PIN:-a release tag}"
fi

if have git; then
    row ok "git" "$(git --version 2>/dev/null | sed -n 's/git version //p')"
else
    row gap "git" "the format and tidy scripts enumerate files with git ls-files"
fi

# ---------------------------------------------------------------------------
# Tier 2 — pass review
# ---------------------------------------------------------------------------
heading 2 "pass review (CI fails you without these)"

# The package that provides clang-format is named differently on every
# platform, and only Debian ships a version-suffixed binary. Name the pinned
# major and the local package rather than a binary that cannot exist here.
case "$HOST" in
    linux)   cf_pkg="apt install clang-format-${CLANG_FORMAT_MAJOR}" ;;
    macos)   cf_pkg="brew install clang-format" ;;
    windows) cf_pkg="pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra" ;;
    *)       cf_pkg="install clang-format ${CLANG_FORMAT_MAJOR}" ;;
esac

if [ -z "$CLANG_FORMAT_MAJOR" ]; then
    row gap "clang-format" "could not read CLANG_FORMAT_MAJOR from scripts/ci/clang-format-select.sh"
elif cf="$(first_of "clang-format-${CLANG_FORMAT_MAJOR}" clang-format)"; then
    cfv="$("$cf" --version 2>/dev/null | sed -n 's/.*clang-format version \([0-9][0-9.]*\).*/\1/p')"
    cfmaj="${cfv%%.*}"
    if [ "$cfmaj" = "$CLANG_FORMAT_MAJOR" ]; then
        row ok "clang-format" "$cfv — pinned major $CLANG_FORMAT_MAJOR (scripts/ci/clang-format-select.sh)"
    else
        row gap "clang-format" "found $cfv, need major $CLANG_FORMAT_MAJOR; the format scripts refuse other majors — $cf_pkg"
    fi
else
    row gap "clang-format" "need major $CLANG_FORMAT_MAJOR — $cf_pkg"
fi

# docs-links.yml is deliberately not path-filtered, so this one gates every PR,
# a docs-only one included. Without it `make docs-links` skips its link half
# with a warning and still exits 0, which reads as a pass.
if have lychee; then
    row ok "lychee" "$(pin_note "$(lychee --version 2>/dev/null | sed -n 's/^lychee //p')" "$LYCHEE_PIN") — make docs-links"
else
    row gap "lychee" "docs-links.yml checks every markdown link and #anchor${LYCHEE_PIN:+ (CI pins $LYCHEE_PIN)}"
fi

# The Lint workflow gates shell, Python and workflow files the way format.yml
# gates C/C++. Each is only needed if your change touches that file type.
if have shellcheck; then
    row ok "shellcheck" "$(pin_note "$(shellcheck --version 2>/dev/null | sed -n 's/^version: //p')" "$SHELLCHECK_PIN"); run with -S warning"
else
    row gap "shellcheck" "the Lint workflow checks every tracked *.sh${SHELLCHECK_PIN:+ (CI pins $SHELLCHECK_PIN)}"
fi
if have ruff; then
    row ok "ruff" "$(pin_note "$(ruff --version 2>/dev/null | sed -n 's/^ruff //p')" "$RUFF_PIN")"
else
    row gap "ruff" "the Lint workflow checks every tracked *.py${RUFF_PIN:+ (CI pins $RUFF_PIN)} — pipx install ruff"
fi
if have actionlint; then
    row ok "actionlint" "$(pin_note "$(actionlint --version 2>/dev/null | head -1)" "$ACTIONLINT_PIN")"
else
    row gap "actionlint" "the Lint workflow checks .github/workflows${ACTIONLINT_PIN:+ (CI pins $ACTIONLINT_PIN)}"
fi

if have python3; then
    row ok "python3" "$(python3 --version 2>/dev/null | sed -n 's/Python //p') — filter-format-files.py, save probes, corpus harness"
else
    row gap "python3" "scripts/ci/filter-format-files.py gates the format check"
fi

# run_tests_docker.sh invokes `docker` by name. On WSL a Docker Desktop
# install can leave a shim that resolves on PATH and then fails at exec,
# so presence alone is not the question — see verify-release.sh.
if have docker; then
    if docker info >/dev/null 2>&1; then
        row ok "container runtime" "docker daemon reachable — ./run_tests_docker.sh"
    else
        row gap "container runtime" "docker resolves but the daemon is unreachable; on WSL check Docker Desktop's WSL integration (docs/TOOLING.md)"
    fi
elif have podman; then
    # Podman speaks every subcommand run_tests_docker.sh uses, but the script
    # calls `docker` by name, so it needs a shim. Say whether podman itself is
    # even usable before suggesting one.
    if podman info >/dev/null 2>&1; then
        row gap "container runtime" "podman works, but run_tests_docker.sh calls 'docker' by name — shim or alias it"
    else
        row gap "container runtime" "podman found but not usable ('podman info' fails); the ASM suite cannot run"
    fi
else
    row gap "container runtime" "./run_tests_docker.sh needs Docker for the ASM equivalence suite"
fi

# ---------------------------------------------------------------------------
# Tier 3 — per lane
# ---------------------------------------------------------------------------
heading 3 "per lane (required only if you work on that lane)"

# ASM equivalence built on the host instead of in the container
if have objcopy; then
    row ok "objcopy" "LBA2_BUILD_ASM_EQUIV_TESTS=ON on the host"
else
    row gap "objcopy" "binutils — only for a host build with LBA2_BUILD_ASM_EQUIV_TESTS=ON"
fi
if printf 'int main(void){return 0;}' | \
   "${cxx:-c++}" -m32 -x c - -o /dev/null >/dev/null 2>&1; then
    row ok "32-bit runtime" "-m32 links — host ASM equivalence possible"
elif [ "$HOST" = windows ]; then
    # UCRT64 and MINGW64 are 64-bit only; 32-bit is a separate MSYS2
    # environment, not a package you add to this one.
    row gap "32-bit runtime" "-m32 is unsupported here; 32-bit needs the MINGW32 environment"
else
    row gap "32-bit runtime" "gcc-multilib/g++-multilib; the container has it, hosts usually don't"
fi
if have uasm; then
    row ok "uasm" "$(uasm -? </dev/null 2>&1 | head -1)"
else
    row gap "uasm" "ENABLE_ASM=ON only; the container fetches ${UASM_PIN:-the pinned build} itself"
fi

# Releasing
if have gh; then
    if gh auth status >/dev/null 2>&1; then
        row ok "gh" "authenticated — release upload, verify-release.sh"
    else
        row gap "gh" "installed but not authenticated (gh auth login); verify-release.sh requires auth"
    fi
else
    row gap "gh" "docs/RELEASING.md, scripts/dev/verify-release.sh"
fi
if have git-cliff; then
    row ok "git-cliff" "CHANGELOG generation (docs/RELEASING.md)"
else
    row gap "git-cliff" "only from the second release on (docs/RELEASING.md)"
fi
if have tar; then
    row ok "tar" "linux tarball bundling, verify-release.sh"
else
    row gap "tar" "scripts/packaging/bundle-linux-tarball.sh"
fi
if have zip; then
    row ok "zip" "windows ZIP bundling"
elif have python3; then
    row ok "zip (python3 fallback)" "bundle-windows.sh falls back to python3 zipfile"
else
    row gap "zip" "bundle-windows.sh needs zip or python3"
fi

# Cross-compiling Windows artifacts from a Unix host. Meaningless on Windows,
# where the native MSYS2 presets are the supported path instead.
if [ "$HOST" != windows ]; then
    if have i686-w64-mingw32-gcc; then
        row ok "mingw-w64" "cmake --preset cross_linux2win"
    else
        row gap "mingw-w64" "only for the cross_linux2win preset (docs/RELEASING.md)"
    fi
fi

# macOS-only bundling
if [ "$HOST" = macos ]; then
    if have hdiutil; then
        row ok "hdiutil" "DMG creation (bundle-macos.sh)"
    else
        row gap "hdiutil" "bundle-macos.sh requires a macOS host"
    fi
    if have xcrun; then
        row ok "xcrun" "Xcode command-line tools"
    else
        row gap "xcrun" "xcode-select --install"
    fi
fi

# Android
ndk_dir="${ANDROID_NDK:-}"
if [ -z "$ndk_dir" ]; then
    ndk_dir="$(ls -d "$HOME"/Android/Sdk/ndk/* 2>/dev/null | sort -V | tail -1)"
fi
if [ -n "$ndk_dir" ] && [ -d "$ndk_dir" ]; then
    # NDK install dirs are versioned 28.2.13676358, where the major is the "r".
    ndk_name="$(basename "$ndk_dir")"
    ndk_have="${ndk_name%%.*}"
    ndk_want="${NDK_MIN#r}"; ndk_want="${ndk_want%+}"
    if [ -n "$ndk_want" ] && [ -n "${ndk_have##*[!0-9]*}" ] \
       && [ "$ndk_have" -lt "$ndk_want" ] 2>/dev/null; then
        row gap "Android NDK" "r$ndk_have found, need $NDK_MIN (scripts/dev/build-android.sh)"
    else
        row ok "Android NDK" "r$ndk_have — floor ${NDK_MIN:-r28+} (scripts/dev/build-android.sh)"
    fi
else
    row gap "Android NDK" "need ${NDK_MIN:-r28+}; set ANDROID_NDK (docs/ANDROID.md)"
fi
sdk_root="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
bt="$(ls -d "$sdk_root"/build-tools/* 2>/dev/null | sort -V | tail -1)"
if [ -n "$bt" ]; then
    row ok "Android build-tools" "$(basename "$bt") — aapt2, zipalign, apksigner"
else
    row gap "Android build-tools" "APK bundling only (scripts/packaging/bundle-android.sh)"
fi
if have javac; then
    row ok "javac" "$(javac -version 2>&1 | sed -n 's/javac //p') — compiles the SDL3 Java activity into classes.dex"
else
    row gap "javac" "JDK${JDK_VER:+ $JDK_VER} (reusable-build-android.yml); APK bundling only"
fi
if have adb; then
    row ok "adb" "install and log on device"
else
    row gap "adb" "on-device install only (docs/ANDROID.md)"
fi

# Runtime, not build: the Linux game-data picker
if [ "$HOST" = linux ]; then
    if have zenity; then
        row ok "zenity" "game-data folder picker"
    elif ls /usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.* >/dev/null 2>&1; then
        row ok "xdg-desktop-portal" "game-data folder picker"
    else
        row gap "folder picker" "zenity or an xdg-desktop-portal backend (docs/GAME_DATA.md)"
    fi
fi

# ---------------------------------------------------------------------------
# Tier 4 — faster, not required
# ---------------------------------------------------------------------------
heading 4 "faster, not required (nothing breaks without these)"

if rct="$(first_of run-clang-tidy "run-clang-tidy-${CLANG_FORMAT_MAJOR:-18}")"; then
    row ok "run-clang-tidy" "$rct — scripts/ci/run-clang-tidy.sh"
else
    row info "run-clang-tidy" "clang-tools-extra; never runs in CI (.clang-tidy)"
fi
if python3 -c "from PIL import Image" >/dev/null 2>&1; then
    row ok "Pillow" "screenshot comparison in tests/automation"
else
    row info "Pillow" "pip install Pillow — image asserts skip without it (tests/automation/lib.sh)"
fi
if have convert || have magick; then
    row ok "ImageMagick" "polyrec PPM to PNG (tests/SNAPSHOT/render_polyrec.sh)"
else
    row info "ImageMagick" "polyrec keeps the PPMs and skips PNG conversion"
fi
if have act; then
    row ok "act" "run Linux CI jobs locally"
else
    row info "act" "Linux jobs only; macOS and Windows runners cannot be emulated"
fi
if dbg="$(first_of gdb lldb)"; then
    row ok "debugger" "$dbg"
else
    row info "debugger" "gdb or lldb"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
plural() { [ "$1" = 1 ] && echo gap || echo gaps; }

printf '\n'
if [ "$TIER1_GAPS" -gt 0 ]; then
    printf '%sTier 1 has %d %s — this clone cannot build and run.%s\n' \
        "$C_BAD" "$TIER1_GAPS" "$(plural "$TIER1_GAPS")" "$C_OFF"
elif want_tier 1; then
    printf '%sTier 1 complete — this clone can build and run.%s\n' "$C_OK" "$C_OFF"
fi
if want_tier 2 && [ "$TIER2_GAPS" -gt 0 ]; then
    printf '%sTier 2 has %d %s — CI will catch what you cannot check locally.%s\n' \
        "$C_WARN" "$TIER2_GAPS" "$(plural "$TIER2_GAPS")" "$C_OFF"
fi
printf '%sTiers 3 and 4 are informational. Full table: docs/TOOLING.md%s\n' \
    "$C_DIM" "$C_OFF"

[ "$TIER1_GAPS" -gt 0 ] && exit 1
[ "$STRICT" = 1 ] && [ "$TIER2_GAPS" -gt 0 ] && exit 1
exit 0
