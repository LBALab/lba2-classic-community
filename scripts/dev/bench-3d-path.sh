#!/usr/bin/env bash
# What the 3D maths costs, float path against integer path (tools/bench_3d_path.cpp).
#
#   scripts/dev/bench-3d-path.sh             # this machine: Release build of the bench target
#   scripts/dev/bench-3d-path.sh --android   # a phone over adb, armeabi-v7a and arm64-v8a
#
# --android needs adb with one device attached and the NDK: ANDROID_NDK_HOME, or the newest
# under $ANDROID_HOME/ndk (default ~/Android/Sdk). The bench is a plain executable pushed
# to /data/local/tmp and removed afterwards; no app is installed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [ "${1:-}" != "--android" ]; then
    B="$ROOT/build-bench"
    cmake -S "$ROOT" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLBA2_BUILD_BENCH=ON > /dev/null
    cmake --build "$B" --target bench_3d_path > /dev/null
    exec "$B/tools/bench_3d_path"
fi

SDK="${ANDROID_HOME:-$HOME/Android/Sdk}"
NDK="${ANDROID_NDK_HOME:-$(find "$SDK/ndk" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -1)}"
[ -d "$NDK" ] || { echo "no NDK found (set ANDROID_NDK_HOME)" >&2; exit 1; }
BIN="$(find "$NDK/toolchains/llvm/prebuilt" -mindepth 2 -maxdepth 2 -name bin -type d | head -1)"
command -v adb > /dev/null || { echo "adb not on PATH" >&2; exit 1; }

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

echo "device: $(adb shell getprop ro.product.model | tr -d '\r'), NDK $(basename "$NDK")"
for abi in armeabi-v7a arm64-v8a; do
    case "$abi" in
    armeabi-v7a) cxx="$BIN/armv7a-linux-androideabi24-clang++" ;;
    arm64-v8a) cxx="$BIN/aarch64-linux-android24-clang++" ;;
    esac
    # LIB386/3D compiled in directly; the narrowing flag matches the project's build of SINTAB.
    "$cxx" -std=c++17 -O3 -Wno-c++11-narrowing -static-libstdc++ -DLBA2_BENCH_STANDALONE \
        -I"$ROOT/LIB386/H" "$ROOT/tools/bench_3d_path.cpp" "$ROOT"/LIB386/3D/*.CPP -o "$OUT/bench-$abi"
    adb push "$OUT/bench-$abi" /data/local/tmp/ > /dev/null
    echo "== $abi"
    adb shell "chmod 755 /data/local/tmp/bench-$abi && /data/local/tmp/bench-$abi; rm -f /data/local/tmp/bench-$abi"
done
