#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Shared helpers for driving an Android device from the host.
#
# Sourced, never executed. It deliberately does not set -e: a caller tallies
# pass/fail/skip and needs every check to run, which is the same reason
# check-android-userdir.sh sets -uo pipefail without -e.
#
# The one thing worth setting up before anything here is used: run the adb
# SERVER on Windows and a Linux adb CLIENT in WSL, so every path in the design
# stays a Linux path and wslpath never enters it.
#
#   ADB_SERVER_SOCKET=tcp:<windows-host-ip>:5037 adb devices
#
# Without that, ADB may be pointed at the Windows binary directly, which cannot
# open a /mnt/... path; host_to_adb_path below converts for that case.
#
# Env:
#   ADB              adb binary (default: adb)
#   ANDROID_SERIAL   pins the target when more than one is attached
#   LBA2_ANDROID_DIR device scratch root (default: /data/local/tmp/lba2cc)
# ---------------------------------------------------------------------------

ADB="${ADB:-adb}"
ANDROID_DEVICE_DIR="${LBA2_ANDROID_DIR:-/data/local/tmp/lba2cc}"

# Probe results, filled in by android_probe. Empty until then.
ANDROID_ABI_ON_DEVICE=""
ANDROID_IS_EMULATOR=""
ANDROID_EXIT_CODES_PROPAGATE=""
# Every note is kept as well as printed, so a caller that caches a probe result
# can replay the reason. A refusal that arrives as a bare exit 1 on the second
# run is worse than no cache at all.
ANDROID_PROBE_LOG=""

android_note() {
    echo "[android] $*" >&2
    ANDROID_PROBE_LOG="${ANDROID_PROBE_LOG}[android] $*
"
}

# `test -e` through the device shell rather than `ls`, so a permission error and
# a missing file are not the same answer.
exists_on_device() {
    [[ "$("$ADB" shell "[ -e '$1' ] && echo yes || echo no" 2>/dev/null | tr -d '\r')" == "yes" ]]
}

device_prop() { "$ADB" shell getprop "$1" 2>/dev/null | tr -d '\r'; }

# adb is sometimes the Windows binary, because that is what owns the emulator,
# and it cannot open a /mnt/... path. Convert only when we can tell that is the
# case, so a Linux client keeps its own spelling.
host_to_adb_path() {
    if [[ "$ADB" == *.exe ]] && command -v wslpath >/dev/null 2>&1; then
        wslpath -w "$1"
    else
        printf '%s' "$1"
    fi
}

# Everything below this line depends on the probe having run. It answers four
# questions, and each one has a way of being wrong that costs a whole session:
#
#   1. Is anything attached, and is it the one we mean?
#   2. Is it ARM? An x86_64 image has 80-bit long double exactly like the
#      desktop, so it CANNOT reproduce an ARM rounding divergence. A lane that
#      ran there would pass and mean nothing, which is worse than not running.
#   3. Can /data/local/tmp execute? Some hardened OEM builds say no under
#      SELinux. That is a SKIP, not a FAIL.
#   4. Does `adb shell` propagate the exit code? Shell protocol v2 does, and
#      minSdkVersion 24 is exactly its floor, but an old adb can negotiate v1
#      and silently return 0 for everything.
#
# Returns 0 when the device is usable, 1 when it is not, 77 when it is present
# but cannot host the lane (the suite's SKIP convention).
android_probe() {
    local serials count
    # tr -d '\r' before anything parses this. The Windows adb.exe emits CRLF, so
    # a bare `$2=="device"` compares against "device\r" and silently matches
    # nothing: every device reads as no device attached.
    serials="$("$ADB" devices 2>/dev/null | tr -d '\r' | awk 'NR>1 && $2=="device" {print $1}')"
    count="$(printf '%s\n' "$serials" | grep -c . )"

    if [[ "$count" -eq 0 ]]; then
        android_note "no device or emulator attached"
        return 1
    fi
    if [[ "$count" -gt 1 && -z "${ANDROID_SERIAL:-}" ]]; then
        android_note "$count targets attached and ANDROID_SERIAL is not set:"
        printf '%s\n' "$serials" | sed 's/^/[android]   /' >&2
        android_note "pin one, or two runs will interleave on the same device"
        return 1
    fi
    if [[ -z "${ANDROID_SERIAL:-}" ]]; then
        ANDROID_SERIAL="$serials"
        export ANDROID_SERIAL
    fi

    ANDROID_ABI_ON_DEVICE="$(device_prop ro.product.cpu.abi)"
    case "$ANDROID_ABI_ON_DEVICE" in
    arm64-v8a | armeabi-v7a | armeabi) ;;
    "")
        android_note "could not read ro.product.cpu.abi from $ANDROID_SERIAL"
        return 1
        ;;
    *)
        android_note "target $ANDROID_SERIAL is $ANDROID_ABI_ON_DEVICE, not ARM."
        android_note "x86 and x86_64 Android carry 80-bit long double, the same as this"
        android_note "host, so they cannot show an ARM rounding difference. Refusing to"
        android_note "run: a green result here would mean nothing. Use an arm64-v8a"
        android_note "system image or a physical ARM device."
        return 1
        ;;
    esac

    if [[ "$(device_prop ro.build.characteristics)" == *emulator* ]] ||
        [[ "$(device_prop ro.kernel.qemu)" == "1" ]] ||
        [[ "$ANDROID_SERIAL" == emulator-* ]]; then
        ANDROID_IS_EMULATOR=1
    else
        ANDROID_IS_EMULATOR=0
    fi

    "$ADB" shell "mkdir -p '$ANDROID_DEVICE_DIR'" >/dev/null 2>&1
    local probe="$ANDROID_DEVICE_DIR/.exec-probe"
    "$ADB" shell "printf '#!/system/bin/sh\nexit 7\n' > '$probe' && chmod 755 '$probe'" >/dev/null 2>&1
    "$ADB" shell "'$probe'" >/dev/null 2>&1
    local rc=$?
    "$ADB" shell "rm -f '$probe'" >/dev/null 2>&1
    if [[ $rc -eq 7 ]]; then
        ANDROID_EXIT_CODES_PROPAGATE=1
    elif [[ $rc -eq 0 ]]; then
        # The script ran but the code came back 0, or it did not run at all and
        # the shell still said 0. Tell those apart before blaming the device.
        if "$ADB" shell "echo ok" 2>/dev/null | grep -q ok; then
            ANDROID_EXIT_CODES_PROPAGATE=0
            android_note "adb shell is not propagating exit codes (shell protocol v1?)."
            android_note "Callers must read a status marker instead of \$?."
        else
            android_note "adb shell is not usable on $ANDROID_SERIAL"
            return 1
        fi
    else
        android_note "cannot execute from $ANDROID_DEVICE_DIR (SELinux?): rc=$rc"
        return 77
    fi

    return 0
}

# An emulated arm64 image runs under QEMU TCG on an x86 host, which is 10x to
# 50x slower than hardware. --fixed-dt pins game time rather than wall time, so
# slowness never changes a result, but a timeout tuned for a desktop turns every
# run into a false failure.
android_timeout_scale() {
    if [[ "$ANDROID_IS_EMULATOR" == "1" ]]; then echo 20; else echo 1; fi
}

# Push only when the device does not already hold this exact content. ctest
# calls the exec shim once per test, so re-pushing SDL3 seventy times would cost
# more than the tests do. The marker name carries the hash, so a changed file
# misses the cache rather than being served stale.
android_push_cached() {
    local src="$1" dest_dir="$2" mode="${3:-644}"
    local base hash marker
    base="$(basename "$src")"
    hash="$(sha256sum "$src" | cut -c1-16)"
    marker="$dest_dir/.cached-$base-$hash"

    if exists_on_device "$marker"; then
        return 0
    fi
    "$ADB" shell "mkdir -p '$dest_dir'" >/dev/null 2>&1
    if ! "$ADB" push "$(host_to_adb_path "$src")" "$dest_dir/$base" >/dev/null 2>&1; then
        android_note "push failed: $src"
        return 1
    fi
    "$ADB" shell "chmod $mode '$dest_dir/$base' && rm -f '$dest_dir'/.cached-'$base'-* && touch '$marker'" >/dev/null 2>&1
    return 0
}

# The probe costs several adb round trips and a test run calls it once per
# test or engine run, so cache it. Keyed on the boot id as well as the serial: a reboot or
# a different device must miss, or we would stage onto assumptions that belonged
# to another machine.
android_probe_cached() {
    local boot_id serial state state_dir="${LBA2_ANDROID_STATE:-${TMPDIR:-/tmp}/lba2cc-android-exec}"
    mkdir -p "$state_dir" 2>/dev/null
    serial="$("$ADB" get-serialno 2>/dev/null | tr -d '\r')"
    boot_id="$("$ADB" shell cat /proc/sys/kernel/random/boot_id 2>/dev/null | tr -d '\r-')"
    if [[ -z "$serial" || -z "$boot_id" ]]; then
        android_probe
        return $?
    fi
    state="$state_dir/$serial.$boot_id"
    if [[ -r "$state" ]]; then
        # shellcheck disable=SC1090
        . "$state"
        # Replay why, not just the code. Under ctest the exec shim runs once
        # per test; a cached refusal that printed nothing would give a column
        # of bare failures and no reason anywhere in the output.
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

# Where the runtime libraries come from. Read the build's own CMakeCache rather
# than the environment: the cache is what the binary was actually linked
# against, and an ambient ANDROID_NDK pointing somewhere else would stage an STL
# that does not match. bundle-android.sh reads the cache for the same reason.
android_find_cache() {
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

android_cache_value() { sed -n "s|^$2:[^=]*=||p" "$1" | head -1; }

# Copy what an Android executable needs beside it: libc++_shared.so and, when
# the build names one, libSDL3.so. Checks the ABI it was built for against the
# target. Needs android_probe first.
android_stage_runtime() {
    local cache ndk sdl3_cfg sdl3_so stl triple d
    cache="$(android_find_cache "$1")"
    if [[ -z "$cache" ]]; then
        android_note "no CMakeCache.txt above $1; cannot find the runtime libs"
        return 1
    fi

    # NDK r28 caches neither CMAKE_ANDROID_NDK nor CMAKE_ANDROID_ARCH_ABI, only
    # the toolchain file and the ANDROID_ABI given on the command line.
    ndk="$(android_cache_value "$cache" CMAKE_ANDROID_NDK)"
    if [[ -z "$ndk" ]]; then
        ndk="$(android_cache_value "$cache" CMAKE_TOOLCHAIN_FILE)"
        ndk="${ndk%/build/cmake/android.toolchain.cmake}"
    fi
    ANDROID_BUILD_ABI="$(android_cache_value "$cache" CMAKE_ANDROID_ARCH_ABI)"
    [[ -n "$ANDROID_BUILD_ABI" ]] || ANDROID_BUILD_ABI="$(android_cache_value "$cache" ANDROID_ABI)"
    sdl3_cfg="$(android_cache_value "$cache" SDL3_DIR)"

    case "$ANDROID_BUILD_ABI" in
    arm64-v8a) triple=aarch64-linux-android ;;
    armeabi-v7a) triple=arm-linux-androideabi ;;
    *)
        android_note "unexpected ABI '$ANDROID_BUILD_ABI'"
        return 1
        ;;
    esac

    if [[ "$ANDROID_BUILD_ABI" != "$ANDROID_ABI_ON_DEVICE" && ! ("$ANDROID_BUILD_ABI" == armeabi-v7a && "$ANDROID_ABI_ON_DEVICE" == arm64-v8a) ]]; then
        android_note "built $ANDROID_BUILD_ABI but the target is $ANDROID_ABI_ON_DEVICE"
        return 1
    fi

    # The prebuilt folder is named after the build host (linux-x86_64,
    # darwin-x86_64, windows-x86_64), so take whichever one this NDK has.
    stl=""
    for d in "$ndk"/toolchains/llvm/prebuilt/*/sysroot/usr/lib/"$triple"; do
        [[ -f "$d/libc++_shared.so" ]] && stl="$d/libc++_shared.so" && break
    done
    if [[ -z "$stl" ]]; then
        android_note "no libc++_shared.so for $triple under $ndk"
        return 1
    fi
    android_push_cached "$stl" "$ANDROID_DEVICE_DIR/lib" 755 || return 1

    # Tests that never link SDL3 run without it, so a build with no SDL3_DIR is
    # fine. One that names an SDL3 install which is not there is not.
    if [[ -n "$sdl3_cfg" ]]; then
        sdl3_so="${sdl3_cfg%/lib/cmake/SDL3}/lib/libSDL3.so"
        if [[ ! -f "$sdl3_so" ]]; then
            android_note "SDL3_DIR is $sdl3_cfg but $sdl3_so is missing"
            return 1
        fi
        android_push_cached "$sdl3_so" "$ANDROID_DEVICE_DIR/lib" 755 || return 1
    fi
    return 0
}
