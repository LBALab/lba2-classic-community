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

NDK="$(cache_value "$CACHE" CMAKE_ANDROID_NDK)"
ABI="$(cache_value "$CACHE" CMAKE_ANDROID_ARCH_ABI)"
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

STL="$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$TRIPLE/libc++_shared.so"
SDL3_SO="${SDL3_CFG%/lib/cmake/SDL3}/lib/libSDL3.so"

[[ -f "$STL" ]] && android_push_cached "$STL" "$ANDROID_DEVICE_DIR/lib" 755
[[ -f "$SDL3_SO" ]] && android_push_cached "$SDL3_SO" "$ANDROID_DEVICE_DIR/lib" 755

BASE="$(basename "$BIN")"
if ! android_push_cached "$BIN" "$ANDROID_DEVICE_DIR/bin" 755; then
    exit 1
fi

# Give each run its own scratch and TMPDIR. A test that leaves a file behind
# must not be able to change the next test's answer, and two runs sharing one
# device must not be able to see each other's temp files.
RUN_DIR="$ANDROID_DEVICE_DIR/run/$BASE.$$"

quoted=""
for a in "$@"; do
    quoted+=" $(printf '%q' "$a")"
done

# Run with cwd inside the scratch dir. Exit code comes back through adb shell
# (protocol v2); the probe already established whether that is true, and a
# device where it is not gets a marker instead of a silent 0.
if [[ "$ANDROID_EXIT_CODES_PROPAGATE" == "1" ]]; then
    "$ADB" shell "mkdir -p '$RUN_DIR' && cd '$RUN_DIR' && TMPDIR='$RUN_DIR' LD_LIBRARY_PATH='$ANDROID_DEVICE_DIR/lib' '$ANDROID_DEVICE_DIR/bin/$BASE'$quoted"
    rc=$?
else
    out="$("$ADB" shell "mkdir -p '$RUN_DIR' && cd '$RUN_DIR' && TMPDIR='$RUN_DIR' LD_LIBRARY_PATH='$ANDROID_DEVICE_DIR/lib' '$ANDROID_DEVICE_DIR/bin/$BASE'$quoted; echo __RC=\$?" 2>&1)"
    printf '%s\n' "${out%__RC=*}"
    rc="${out##*__RC=}"
    rc="$(printf '%s' "$rc" | tr -d '\r\n ')"
    [[ "$rc" =~ ^[0-9]+$ ]] || rc=1
fi

"$ADB" shell "rm -rf '$RUN_DIR'" >/dev/null 2>&1
exit "$rc"
