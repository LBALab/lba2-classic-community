#!/usr/bin/env bash
# Build the UWP spike, deploy it to an Xbox in Developer Mode as a loose folder (no
# packaging, no signing), run it, then save a screenshot and the app's log and stop it.
#
#   LLVM_MINGW_ROOT=... tools/uwp_spike/xbox-run.sh [SECONDS] [OUT_DIR]
#
# Talks to the console through the homelab's Device Portal helper (XBOX_SH, default
# ~/code/homelab/ops/xbox.sh), which holds the login and the pinned certificate. The
# console needs a signed-in user, or the launch waits on a sign-in prompt on the TV.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
seconds="${1:-8}"
out="${2:-${TMPDIR:-/tmp}/lba2cc-uwp-run}"
xbox="${XBOX_SH:-$HOME/code/homelab/ops/xbox.sh}"
cxx="${LLVM_MINGW_ROOT:?set LLVM_MINGW_ROOT to an unpacked llvm-mingw release}/bin/x86_64-w64-mingw32uwp-clang++"
full="LBA2CC.UwpSpike_0.0.1.0_x64__bzkng0es5r9z8"

b64() { printf '%s' "$1" | base64 | tr -d '\n' | python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.stdin.read(), safe=""))'; }
# A running copy holds its .exe open, and the deploy then fails with HTTP 500.
stop() { "$xbox" api DELETE "/api/taskmanager/app?package=$(b64 "$full")" >/dev/null 2>&1 || true; }

stage="$out/lba2cc-uwp-spike"
rm -rf "$stage" && mkdir -p "$stage"
"$cxx" -std=gnu++98 -Wall -Wextra -O2 -municode -mwindows "$here/uwp_spike.cpp" -o "$stage/uwp_spike.exe" \
    -ld3d11 -ldxgi -lruntimeobject -static
cp -R "$here/AppxManifest.xml" "$here/Assets" "$stage/"

stop
"$xbox" deploy "$stage"
"$xbox" launch "$full"
sleep "$seconds"
"$xbox" screenshot "$out/screenshot.png"
rm -rf "${out:?}/logs" && "$xbox" pull-logs "$full" "$out/logs" >/dev/null
stop
echo "--- $out/logs/$full/LocalState/uwp_spike.log"
cat "$out/logs/$full/LocalState/uwp_spike.log"
echo "--- screenshot: $out/screenshot.png"
