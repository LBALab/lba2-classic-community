#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Run one cross-built ARM executable on an attached Android target.
#
# Written to be CMake's CMAKE_CROSSCOMPILING_EMULATOR, so plain `ctest` on the
# host drives the device and no new test runner has to exist:
#
#   cmake -S . -B out/build/android_arm64_tests -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
#       -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 -DANDROID_STL=c++_shared \
#       -DSDL3_DIR=$SDL3_ANDROID_DIR/lib/cmake/SDL3 \
#       -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF \
#       -DCMAKE_CROSSCOMPILING_EMULATOR=$PWD/scripts/dev/android-exec.sh
#   ctest --test-dir out/build/android_arm64_tests --output-on-failure
#
# LBA2_BUILD_ASM_EQUIV_TESTS=OFF is load-bearing: it defaults ON and those tests
# need UASM and x86.
#
# It can also be run by hand, which is the fastest way to read one result:
#   scripts/dev/android-exec.sh out/build/android_arm64_tests/tests/numeric_contract/test_numeric_contract
#
# Usage: android-exec.sh <arm-executable> [args...]
# Exit:  the executable's own exit code; 77 (SKIP) when the device cannot host
#        the run; 1 when staging failed.
# ---------------------------------------------------------------------------
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/dev/android-lib.sh
. "$HERE/android-lib.sh"

BIN="${1:-}"
if [[ -z "$BIN" || ! -f "$BIN" ]]; then
    echo "usage: android-exec.sh <arm-executable> [args...]" >&2
    exit 2
fi
shift

# The probe costs several adb round trips and ctest calls this once per test, so
# cache it for the run. Keyed on the boot id as well as the serial: a reboot or
# a different device must miss, or we would stage onto assumptions that belonged
# to another machine.
STATE_DIR="${TMPDIR:-/tmp}/lba2cc-android-exec"
mkdir -p "$STATE_DIR" 2>/dev/null

probe_cached() {
    local boot_id serial state
    serial="$("$ADB" get-serialno 2>/dev/null | tr -d '\r')"
    boot_id="$("$ADB" shell cat /proc/sys/kernel/random/boot_id 2>/dev/null | tr -d '\r-')"
    if [[ -z "$serial" || -z "$boot_id" ]]; then
        android_probe
        return $?
    fi
    state="$STATE_DIR/$serial.$boot_id"
    if [[ -r "$state" ]]; then
        # shellcheck disable=SC1090
        . "$state"
        # Replay why, not just the code. Under ctest this shim runs seventy
        # times; a cached refusal that printed nothing would give seventy bare
        # failures and no reason anywhere in the output.
        [[ "${ANDROID_PROBE_RC:-1}" -ne 0 && -n "${ANDROID_PROBE_LOG:-}" ]] &&
            printf '%s' "$ANDROID_PROBE_LOG" >&2
        return "${ANDROID_PROBE_RC:-1}"
    fi
    android_probe
    local rc=$?
    {
        echo "ANDROID_SERIAL='${ANDROID_SERIAL:-}'"
        echo "ANDROID_ABI_ON_DEVICE='$ANDROID_ABI_ON_DEVICE'"
        echo "ANDROID_IS_EMULATOR='$ANDROID_IS_EMULATOR'"
        echo "ANDROID_EXIT_CODES_PROPAGATE='$ANDROID_EXIT_CODES_PROPAGATE'"
        echo "ANDROID_PROBE_RC=$rc"
        printf "ANDROID_PROBE_LOG=%q\n" "$ANDROID_PROBE_LOG"
    } >"$state"
    export ANDROID_SERIAL
    return $rc
}

probe_cached
probe_rc=$?
if [[ $probe_rc -eq 77 ]]; then
    echo "android-exec: device cannot execute from $ANDROID_DEVICE_DIR, skipping" >&2
    exit 77
fi
if [[ $probe_rc -ne 0 ]]; then
    exit 1
fi

# Where the runtime libraries come from. Read the build's own CMakeCache rather
# than the environment: the cache is what the binary was actually linked
# against, and an ambient ANDROID_NDK pointing somewhere else would stage an STL
# that does not match. bundle-android.sh reads the cache for the same reason.
find_cache() {
    local d
    d="$(cd "$(dirname "$1")" && pwd)"
    while [[ "$d" != "/" ]]; do
        if [[ -f "$d/CMakeCache.txt" ]]; then
            printf '%s' "$d/CMakeCache.txt"
            return 0
        fi
        d="$(dirname "$d")"
    done
    return 1
}

cache_value() { sed -n "s|^$2:[^=]*=||p" "$1" | head -1; }

CACHE="$(find_cache "$BIN")"
if [[ -z "$CACHE" ]]; then
    echo "android-exec: no CMakeCache.txt above $BIN; cannot find the runtime libs" >&2
    exit 1
fi

# NDK r28 caches neither CMAKE_ANDROID_NDK nor CMAKE_ANDROID_ARCH_ABI, only
# the toolchain file and the ANDROID_ABI given on the command line.
NDK="$(cache_value "$CACHE" CMAKE_ANDROID_NDK)"
if [[ -z "$NDK" ]]; then
    NDK="$(cache_value "$CACHE" CMAKE_TOOLCHAIN_FILE)"
    NDK="${NDK%/build/cmake/android.toolchain.cmake}"
fi
ABI="$(cache_value "$CACHE" CMAKE_ANDROID_ARCH_ABI)"
[[ -n "$ABI" ]] || ABI="$(cache_value "$CACHE" ANDROID_ABI)"
SDL3_CFG="$(cache_value "$CACHE" SDL3_DIR)"

case "$ABI" in
arm64-v8a) TRIPLE=aarch64-linux-android ;;
armeabi-v7a) TRIPLE=arm-linux-androideabi ;;
*)
    echo "android-exec: unexpected CMAKE_ANDROID_ARCH_ABI '$ABI'" >&2
    exit 1
    ;;
esac

if [[ "$ABI" != "$ANDROID_ABI_ON_DEVICE" && ! ("$ABI" == armeabi-v7a && "$ANDROID_ABI_ON_DEVICE" == arm64-v8a) ]]; then
    echo "android-exec: built $ABI but the target is $ANDROID_ABI_ON_DEVICE" >&2
    exit 1
fi

# The prebuilt folder is named after the build host (linux-x86_64,
# darwin-x86_64, windows-x86_64), so take whichever one this NDK has.
STL=""
for d in "$NDK"/toolchains/llvm/prebuilt/*/sysroot/usr/lib/"$TRIPLE"; do
    [[ -f "$d/libc++_shared.so" ]] && STL="$d/libc++_shared.so" && break
done
if [[ -z "$STL" ]]; then
    echo "android-exec: no libc++_shared.so for $TRIPLE under $NDK" >&2
    exit 1
fi
android_push_cached "$STL" "$ANDROID_DEVICE_DIR/lib" 755 || exit 1

# Tests that never link SDL3 run without it, so a build with no SDL3_DIR is
# fine. One that names an SDL3 install which is not there is not.
if [[ -n "$SDL3_CFG" ]]; then
    SDL3_SO="${SDL3_CFG%/lib/cmake/SDL3}/lib/libSDL3.so"
    if [[ ! -f "$SDL3_SO" ]]; then
        echo "android-exec: SDL3_DIR is $SDL3_CFG but $SDL3_SO is missing" >&2
        exit 1
    fi
    android_push_cached "$SDL3_SO" "$ANDROID_DEVICE_DIR/lib" 755 || exit 1
fi

# Repo files the tests open at run time, listed by tests/CMakeLists.txt with
# the device root they are compiled to expect on the first line. One marker
# covers the whole set, so a run checks the device once rather than per file.
DATA_LIST="$(dirname "$CACHE")/android-test-data.txt"
if [[ -f "$DATA_LIST" ]]; then
    REPO_ROOT="$(cd "$HERE/../.." && pwd)"
    data_device_dir="$(head -1 "$DATA_LIST")"
    if [[ "$data_device_dir" != "$ANDROID_DEVICE_DIR" ]]; then
        echo "android-exec: tests were built for $data_device_dir but LBA2_ANDROID_DIR is $ANDROID_DEVICE_DIR" >&2
        exit 1
    fi
    DATA_ROOT="$ANDROID_DEVICE_DIR/repo"
    data_files=()
    while IFS= read -r rel; do
        [[ -n "$rel" ]] && data_files+=("$rel")
    done < <(tail -n +2 "$DATA_LIST")
    if [[ ${#data_files[@]} -gt 0 ]]; then
        data_hash="$(cd "$REPO_ROOT" && cat "${data_files[@]}" | sha256sum | cut -c1-16)"
        if [[ -z "$data_hash" ]] || ! exists_on_device "$DATA_ROOT/.data-$data_hash"; then
            for rel in "${data_files[@]}"; do
                if [[ ! -f "$REPO_ROOT/$rel" ]]; then
                    echo "android-exec: test data $rel is missing from $REPO_ROOT" >&2
                    exit 1
                fi
                "$ADB" shell "mkdir -p '$DATA_ROOT/$(dirname "$rel")'" >/dev/null 2>&1
                if ! "$ADB" push "$(host_to_adb_path "$REPO_ROOT/$rel")" "$DATA_ROOT/$rel" >/dev/null 2>&1; then
                    echo "android-exec: push failed: $rel" >&2
                    exit 1
                fi
            done
            "$ADB" shell "rm -f '$DATA_ROOT'/.data-* && touch '$DATA_ROOT/.data-$data_hash'" >/dev/null 2>&1
        fi
    fi
fi

BASE="$(basename "$BIN")"
if ! android_push_cached "$BIN" "$ANDROID_DEVICE_DIR/bin" 755; then
    exit 1
fi

# Give each run its own scratch and TMPDIR, emptied first so a file left
# behind cannot change the next run's answer. The path is fixed per test
# because tests/CMakeLists.txt compiles it into the tests that write scratch
# files (lba2_test_tmp_dir).
RUN_DIR="$ANDROID_DEVICE_DIR/run/$BASE"

quoted=""
for a in "$@"; do
    quoted+=" $(printf '%q' "$a")"
done

# SDL_APP_NAME stands in for the Activity. With no app name set, SDL3 asks
# Java for the executable name the first time anything creates a thread, and
# with no JVM in this process that is a null dereference inside libSDL3.so.
#
# Run with cwd inside the scratch dir. Exit code comes back through adb shell
# (protocol v2); the probe already established whether that is true, and a
# device where it is not gets a marker instead of a silent 0.
if [[ "$ANDROID_EXIT_CODES_PROPAGATE" == "1" ]]; then
    "$ADB" shell "rm -rf '$RUN_DIR' && mkdir -p '$RUN_DIR' && cd '$RUN_DIR' && TMPDIR='$RUN_DIR' SDL_APP_NAME='$BASE' LD_LIBRARY_PATH='$ANDROID_DEVICE_DIR/lib' '$ANDROID_DEVICE_DIR/bin/$BASE'$quoted"
    rc=$?
else
    out="$("$ADB" shell "rm -rf '$RUN_DIR' && mkdir -p '$RUN_DIR' && cd '$RUN_DIR' && TMPDIR='$RUN_DIR' SDL_APP_NAME='$BASE' LD_LIBRARY_PATH='$ANDROID_DEVICE_DIR/lib' '$ANDROID_DEVICE_DIR/bin/$BASE'$quoted; echo __RC=\$?" 2>&1)"
    printf '%s\n' "${out%__RC=*}"
    rc="${out##*__RC=}"
    rc="$(printf '%s' "$rc" | tr -d '\r\n ')"
    [[ "$rc" =~ ^[0-9]+$ ]] || rc=1
fi

"$ADB" shell "rm -rf '$RUN_DIR'" >/dev/null 2>&1
exit "$rc"
