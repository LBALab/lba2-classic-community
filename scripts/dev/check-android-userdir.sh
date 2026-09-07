#!/usr/bin/env bash
# Check on a real device (or emulator) that the game writes where it should.
#
# Usage:
#   scripts/dev/check-android-userdir.sh <apk>
#
# The host tests in tests/discovery settle the policy: which root wins, and that
# copying a user directory up never overwrites or deletes. What they structurally
# cannot see is Android itself -- whether All Files Access actually lets the
# engine write to /sdcard, and whether the config write survives the FUSE-emulated
# filesystem /sdcard really is. Config and save writes go through fsync plus an
# atomic rename (LIB386/SYSTEM/FILES.CPP), which is the one part of this change
# that could quietly corrupt a save rather than fail loudly.
#
# ADB may be overridden for a Windows adb server seen from WSL:
#   ADB=/mnt/c/Android/Sdk/platform-tools/adb.exe scripts/dev/check-android-userdir.sh app.apk
#
# Checks needing the app's private storage report SKIP without root, so this is
# still worth running on a physical device.
set -uo pipefail

ADB="${ADB:-adb}"
PKG="${LBA2_ANDROID_PACKAGE:-org.lbalab.lba2cc}"
ACTIVITY="$PKG/org.libsdl.app.SDLActivity"
SHARED_DIR="/sdcard/lba2cc/user"
APP_EXTERNAL="/sdcard/Android/data/$PKG/files"
INTERNAL="/data/data/$PKG/files"
BOOT_TIMEOUT=120

APK="${1:-}"
if [[ -z "$APK" || ! -f "$APK" ]]; then
    echo "usage: $0 <apk>" >&2
    exit 2
fi

passes=0
failures=0
skips=0

pass() { echo "  PASS  $1"; passes=$((passes + 1)); }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }
skip() { echo "  SKIP  $1"; skips=$((skips + 1)); }

have_root() {
    [[ "$("$ADB" shell id -u 2>/dev/null | tr -d '\r')" == "0" ]]
}

# `test -e` through the shell rather than `ls`, so a permission error and a
# missing file are not the same answer.
exists_on_device() {
    [[ "$("$ADB" shell "[ -e '$1' ] && echo yes || echo no" 2>/dev/null | tr -d '\r')" == "yes" ]]
}

app_pid() { "$ADB" shell pidof "$PKG" 2>/dev/null | tr -d '\r'; }

# Every check below depends on a COLD start. The user directory is resolved once
# per process and cached, so a launch that lands on a process already running
# re-uses the folder that process picked and quietly measures the previous
# state: `am start` answers "result code=3", the game comes to the front, and
# nothing in the boot path runs again. force-stop is not instant, so wait for
# the process to actually go before starting a new one.
launch_and_wait() {  # $1 = the file whose appearance means the boot got far enough
    "$ADB" shell am force-stop "$PKG" >/dev/null 2>&1
    local waited=0
    while [[ -n "$(app_pid)" && $waited -lt 20 ]]; do
        sleep 1
        waited=$((waited + 1))
    done
    if [[ -n "$(app_pid)" ]]; then
        echo "  (could not stop the running app; a cold start is not possible)" >&2
        return 1
    fi

    "$ADB" shell am start -n "$ACTIVITY" >/dev/null 2>&1
    waited=0
    while [[ $waited -lt $BOOT_TIMEOUT ]]; do
        if exists_on_device "$1"; then
            return 0
        fi
        sleep 3
        waited=$((waited + 3))
    done
    return 1
}

# Best effort, and expected to fail on a production device. On an emulator it
# restores root after a reboot dropped it, which decides whether the checks that
# need app-private storage run or report SKIP.
"$ADB" root >/dev/null 2>&1
"$ADB" wait-for-device >/dev/null 2>&1

echo "== install =="
# From nothing, so a leftover build cannot be mistaken for this one. An install
# is refused outright when the APK is older than what is there, or signed by a
# different key, and the whole run would then be measuring the previous build
# while reporting on this one. Check it landed rather than assume it.
"$ADB" uninstall "$PKG" >/dev/null 2>&1
install_out=$("$ADB" install "$APK" 2>&1)
if ! grep -q "Success" <<<"$install_out"; then
    echo "$install_out" | tail -3
    echo "  the APK did not install, so nothing below would be about it" >&2
    exit 1
fi
echo "  installed $(basename "$APK")"
# The engine opens Settings and asks for this on its own; pre-granting keeps the
# run unattended.
"$ADB" shell appops set "$PKG" MANAGE_EXTERNAL_STORAGE allow >/dev/null 2>&1

echo
echo "== 1. a first launch writes to shared external storage =="
"$ADB" shell rm -rf "$SHARED_DIR" "$APP_EXTERNAL" >/dev/null 2>&1
if launch_and_wait "$SHARED_DIR/lba2.cfg"; then
    pass "lba2.cfg landed in $SHARED_DIR"
else
    fail "no lba2.cfg in $SHARED_DIR after ${BOOT_TIMEOUT}s"
fi

# Written through fsync + atomic rename. A rename that silently did nothing on
# FUSE-emulated storage would leave this empty or absent, which is exactly the
# failure a host test cannot reach.
size=$("$ADB" shell "wc -c < '$SHARED_DIR/lba2.cfg'" 2>/dev/null | tr -d '\r ')
if [[ "${size:-0}" -gt 100 ]]; then
    pass "the atomic config write survived emulated storage ($size bytes)"
else
    fail "lba2.cfg is ${size:-missing} bytes: fsync+rename may not work here"
fi

if exists_on_device "$SHARED_DIR/adeline.log"; then
    pass "adeline.log is somewhere a player can send it from"
else
    fail "no adeline.log in $SHARED_DIR"
fi

echo
echo "== 2. the two spellings of one folder are not reported as two save sets =="
# /sdcard is a symlink to /storage/emulated/0, and both are ranked, so a string
# compare would tell every player with saves that a second copy exists somewhere
# else -- and the second copy would be the folder they are already using. Only a
# real device has the symlink, so only a device can check this.
if "$ADB" shell "grep -q 'another set of saves' '$SHARED_DIR/adeline.log'" 2>/dev/null; then
    fail "a second save set was reported on a plain boot with one folder"
else
    pass "one folder under two names reads as one folder"
fi

echo
echo "== 3. a real second save set IS reported, and neither copy is touched =="
# The positive control for the check above. Without it, that one passes just as
# happily if the warning can never fire at all.
#
# Needs root, for a reason worth knowing: a save written into the app-specific
# folder by `adb shell` stays owned by shell, and on API 30+ images the app does
# not see it at all. So the seed has to be handed over with chown, or the engine
# looks at an empty folder and is right to say nothing.
if ! have_root; then
    skip "seeding the app-specific folder needs root (shell-owned files are invisible to the app)"
else
"$ADB" shell "mkdir -p '$APP_EXTERNAL/save'" >/dev/null 2>&1
"$ADB" shell "echo STRANDED > '$APP_EXTERNAL/save/current.lba'" >/dev/null 2>&1
"$ADB" shell "mkdir -p '$SHARED_DIR/save'" >/dev/null 2>&1
"$ADB" shell "echo KEPT > '$SHARED_DIR/save/current.lba'" >/dev/null 2>&1
forkuid=$("$ADB" shell stat -c '%u' "$APP_EXTERNAL" 2>/dev/null | tr -d '\r')
if [[ -n "$forkuid" ]]; then
    "$ADB" shell "chown -R $forkuid '$APP_EXTERNAL/save'" >/dev/null 2>&1
fi

# The log from the launch above is still sitting there, and waiting for a file
# that already exists is not a wait: launch_and_wait would return before the app
# had done anything and the grep below would read the previous boot's banner.
"$ADB" shell rm -f "$SHARED_DIR/adeline.log" >/dev/null 2>&1
if launch_and_wait "$SHARED_DIR/adeline.log"; then
    if "$ADB" shell "grep -q 'another set of saves' '$SHARED_DIR/adeline.log'" 2>/dev/null; then
        pass "the second save set was named in the banner"
    else
        fail "two folders held saves and the banner said nothing"
    fi
    kept=$("$ADB" shell cat "$SHARED_DIR/save/current.lba" 2>/dev/null | tr -d '\r\n')
    stranded=$("$ADB" shell cat "$APP_EXTERNAL/save/current.lba" 2>/dev/null | tr -d '\r\n')
    if [[ "$kept" == "KEPT" && "$stranded" == "STRANDED" ]]; then
        pass "both save sets were left exactly as they were"
    else
        fail "a save changed: chosen='$kept' other='$stranded'"
    fi
else
    fail "no log after ${BOOT_TIMEOUT}s"
fi
# Cleared, or the checks below would migrate from here instead of from the
# folder they are actually about.
"$ADB" shell rm -rf "$APP_EXTERNAL" >/dev/null 2>&1
fi

echo
echo "== 4. nothing is left behind in app-private storage =="
if have_root; then
    if exists_on_device "$INTERNAL/lba2.cfg"; then
        fail "the engine still wrote $INTERNAL/lba2.cfg"
    else
        pass "app-private storage holds no config"
    fi
else
    skip "app-private storage needs root to inspect"
fi

echo
echo "== 5. a user directory left in app-private storage is carried up =="
if have_root; then
    "$ADB" shell am force-stop "$PKG" >/dev/null 2>&1
    "$ADB" shell rm -rf "$SHARED_DIR" >/dev/null 2>&1
    "$ADB" shell "mkdir -p '$INTERNAL/save'" >/dev/null 2>&1
    "$ADB" shell "echo OLD-SAVE > '$INTERNAL/save/current.lba'" >/dev/null 2>&1
    "$ADB" shell "echo Version=1 > '$INTERNAL/lba2.cfg'" >/dev/null 2>&1
    # Ownership matters: files left owned by shell are invisible to the app.
    uid=$("$ADB" shell stat -c '%u' "$INTERNAL" 2>/dev/null | tr -d '\r')
    if [[ -n "$uid" ]]; then
        "$ADB" shell "chown -R $uid:$uid '$INTERNAL'" >/dev/null 2>&1
    fi

    if launch_and_wait "$SHARED_DIR/save/current.lba"; then
        got=$("$ADB" shell cat "$SHARED_DIR/save/current.lba" 2>/dev/null | tr -d '\r\n')
        if [[ "$got" == "OLD-SAVE" ]]; then
            pass "the old save was copied up to $SHARED_DIR"
        else
            fail "copied a save up but it reads '$got'"
        fi
    else
        fail "no save appeared in $SHARED_DIR after ${BOOT_TIMEOUT}s"
    fi

    if exists_on_device "$INTERNAL/save/current.lba"; then
        pass "the original was left where it was"
    else
        fail "the original save is gone: this must copy, never move"
    fi
else
    skip "seeding app-private storage needs root"
fi

echo
echo "== 6. without All Files Access, saves still land somewhere reachable =="
"$ADB" shell am force-stop "$PKG" >/dev/null 2>&1
"$ADB" shell appops set "$PKG" MANAGE_EXTERNAL_STORAGE deny >/dev/null 2>&1
"$ADB" shell rm -rf "$SHARED_DIR" "$APP_EXTERNAL" >/dev/null 2>&1
if launch_and_wait "$APP_EXTERNAL/lba2.cfg"; then
    pass "fell back to $APP_EXTERNAL"
else
    fail "no config in $APP_EXTERNAL after ${BOOT_TIMEOUT}s"
fi

echo
echo "== 7. granting it afterwards brings the folder up rather than losing it =="
# `appops set ... allow` grants the operation, but it does NOT rebuild a running
# app's view of external storage, and neither does a cold start: on API 36 the
# app keeps seeing the restricted view and keeps using the fallback, while
# `appops get` cheerfully reports "allow" (with a rejectTime beside it, which is
# the tell). Granting through the Settings UI rebuilds the view; so does a
# reboot, which is the only one a script can reach.
#
# Opt in. Rebooting somebody's phone should not be a side effect of running a
# check, and this is the slow part of the run.
if [[ "${LBA2_ALLOW_REBOOT:-0}" == "1" ]]; then
    "$ADB" shell am force-stop "$PKG" >/dev/null 2>&1
    "$ADB" shell "mkdir -p '$APP_EXTERNAL/save'" >/dev/null 2>&1
    "$ADB" shell "echo FALLBACK-SAVE > '$APP_EXTERNAL/save/current.lba'" >/dev/null 2>&1
    # Written as shell, so it lands owned by shell and the app may not see it at
    # all on stricter images. Hand it over where we can.
    appuid=$("$ADB" shell stat -c '%u' "$APP_EXTERNAL" 2>/dev/null | tr -d '\r')
    if [[ -n "$appuid" ]]; then
        "$ADB" shell "chown -R $appuid '$APP_EXTERNAL/save'" >/dev/null 2>&1
    fi
    "$ADB" shell appops set "$PKG" MANAGE_EXTERNAL_STORAGE allow >/dev/null 2>&1
    "$ADB" shell rm -rf "$SHARED_DIR" >/dev/null 2>&1

    echo "  rebooting so the app's storage view is rebuilt..."
    "$ADB" reboot
    sleep 20
    "$ADB" wait-for-device
    booted=0
    while [[ $booted -lt 180 ]]; do
        [[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
        sleep 3
        booted=$((booted + 3))
    done
    sleep 15

    if launch_and_wait "$SHARED_DIR/save/current.lba"; then
        got=$("$ADB" shell cat "$SHARED_DIR/save/current.lba" 2>/dev/null | tr -d '\r\n')
        if [[ "$got" == "FALLBACK-SAVE" ]]; then
            pass "the fallback folder was carried up once the permission arrived"
        else
            fail "carried something up, but it reads '$got'"
        fi
    else
        fail "nothing arrived in $SHARED_DIR after ${BOOT_TIMEOUT}s"
    fi
else
    skip "set LBA2_ALLOW_REBOOT=1: appops alone cannot refresh the storage view"
fi

echo
echo "== 8. the folder survives an uninstall =="
# A property of the location, not of the app, so it is asserted with a file this
# script puts there. Deriving it from a save the run happened to leave behind
# would report a cascade every time an earlier check failed.
"$ADB" shell am force-stop "$PKG" >/dev/null 2>&1
"$ADB" shell "mkdir -p '$SHARED_DIR'" >/dev/null 2>&1
"$ADB" shell "echo SURVIVOR > '$SHARED_DIR/uninstall-probe.txt'" >/dev/null 2>&1
if ! exists_on_device "$SHARED_DIR/uninstall-probe.txt"; then
    fail "could not write the probe, so the check below would mean nothing"
else
    "$ADB" uninstall "$PKG" >/dev/null 2>&1
    if exists_on_device "$SHARED_DIR/uninstall-probe.txt"; then
        pass "the folder outlived the uninstall, which is the whole point"
    else
        fail "$SHARED_DIR went with the app"
    fi
    "$ADB" shell rm -f "$SHARED_DIR/uninstall-probe.txt" >/dev/null 2>&1
fi

echo
echo "$passes passed, $failures failed, $skips skipped"
[[ $failures -eq 0 ]]
