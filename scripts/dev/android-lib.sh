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
    "$ADB" shell "chmod $mode '$dest_dir/$base' && rm -f '$dest_dir'/.cached-$base-* && touch '$marker'" >/dev/null 2>&1
    return 0
}
