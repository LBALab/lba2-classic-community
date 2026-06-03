#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Verify an Android APK is safe to load on 16 KB memory-page devices.
#
# Checks three things that together guarantee the native libraries can be
# mmap'd straight from the APK on a 16 KB-page kernel (Android 15+, arm64):
#
#   1. Every .so LOAD segment is 16 KB-aligned (ELF p_align == 2**14).
#      Fixed by NDK r28 / -Wl,-z,max-page-size=16384.
#   2. The .so are STORED uncompressed. extractNativeLibs="false" maps them
#      from the APK, which is impossible if they are deflated. (A zipalign
#      check alone does NOT catch this — it reports compressed entries as OK.)
#   3. zipalign agrees the stored .so sit on 16 KB boundaries in the zip.
#
# Usage:   check-16k-align.sh <app.apk>
# Env:     ANDROID_NDK            (for llvm-readelf)
#          ANDROID_HOME / ANDROID_SDK_ROOT  (for zipalign in build-tools)
# Exit:    0 = 16 KB-safe, 1 = not safe / tooling missing.
# ---------------------------------------------------------------------------
set -euo pipefail

APK="${1:?usage: check-16k-align.sh <app.apk>}"
[ -f "$APK" ] || { echo "check-16k-align: APK not found: $APK" >&2; exit 1; }

NDK="${ANDROID_NDK:?check-16k-align: ANDROID_NDK must be set}"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"

READELF=$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-readelf 2>/dev/null | head -1)
[ -x "$READELF" ] || { echo "check-16k-align: llvm-readelf not found under NDK" >&2; exit 1; }
ZIPALIGN=$(ls -d "$SDK"/build-tools/*/zipalign 2>/dev/null | sort -V | tail -1)
[ -x "$ZIPALIGN" ] || { echo "check-16k-align: zipalign not found under SDK build-tools" >&2; exit 1; }

fail=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# 1+2. Per-.so: stored uncompressed, and every LOAD segment >= 16 KB aligned.
echo "== native libraries =="
mapfile -t solist < <(unzip -Z1 "$APK" 'lib/*/*.so' 2>/dev/null)
[ "${#solist[@]}" -gt 0 ] || { echo "  FAIL: no lib/*/*.so entries in APK" >&2; exit 1; }

for entry in "${solist[@]}"; do
    # compression method (unzip -v column 2): 'Stored' = uncompressed
    method=$(unzip -v "$APK" "$entry" 2>/dev/null | awk -v e="$entry" '$NF==e{print $2}')
    if [ "$method" = "Stored" ]; then
        store_ok="stored"
    else
        store_ok="COMPRESSED(${method:-?})"; fail=1
    fi

    unzip -qo "$APK" "$entry" -d "$tmp"
    minalign=$(( 1 << 30 ))
    while read -r a; do
        d=$(( a ))
        [ "$d" -lt "$minalign" ] && minalign=$d
    done < <("$READELF" -lW "$tmp/$entry" 2>/dev/null | awk '/LOAD/{print $NF}')
    if [ "$minalign" -ge 16384 ]; then
        align_ok="align>=16K(min=$minalign)"
    else
        align_ok="ALIGN<16K(min=$minalign)"; fail=1
    fi

    printf "  %-22s %-18s %s\n" "$(basename "$entry")" "$store_ok" "$align_ok"
done

# 3. zipalign confirms the stored .so sit on 16 KB zip boundaries.
echo "== zipalign -c -P 16 =="
if "$ZIPALIGN" -c -P 16 -v 4 "$APK" >/dev/null 2>&1; then
    echo "  OK"
else
    echo "  FAIL: zip-entry 16 KB alignment check failed"; fail=1
fi

if [ "$fail" -eq 0 ]; then
    echo "16 KB-safe: PASS"
else
    echo "16 KB-safe: FAIL" >&2
    exit 1
fi
