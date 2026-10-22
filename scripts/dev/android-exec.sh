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

android_probe_cached
probe_rc=$?
if [[ $probe_rc -eq 77 ]]; then
    echo "android-exec: device cannot execute from $ANDROID_DEVICE_DIR, skipping" >&2
    exit 77
fi
if [[ $probe_rc -ne 0 ]]; then
    exit 1
fi

android_stage_runtime "$BIN" || exit 1
CACHE="$(android_find_cache "$BIN")"

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
