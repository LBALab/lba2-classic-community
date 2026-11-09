#!/usr/bin/env bash
# Build lba2cc for UWP, deploy it to an Xbox in Developer Mode as a loose folder (no
# packaging, no signing), run it, then save a screenshot and the app's own files and stop it.
#
#   tools/uwp/xbox-run.sh [--register] [SECONDS] [OUT_DIR]
#
# The first run registers the app. Later runs only replace lba2cc.exe in the registered
# folder: registering again wipes the app's LocalState, game data and saves included.
# --register forces it, for a changed manifest or assets.
#
# Needs a build folder configured with the UWP toolchain and PLATFORM_BACKEND=uwp
# (LBA2_UWP_BUILD, default out/build/uwp; see docs/plan/XBOX_PLAN.md). The game data goes
# to the app's LocalState\Game once, with tools/uwp/xbox-push-data.sh. LocalState\args.txt,
# one argument per line, adds arguments to every run. Files the game writes to LocalState
# are copied back, the game data excepted.
#
# Talks to the console through the homelab's Device Portal helper (XBOX_SH, default
# ~/code/homelab/ops/xbox.sh), which holds the login and the pinned certificate. The
# console needs a signed-in user, or the launch waits on a sign-in prompt on the TV.
set -euo pipefail

register=0
if [ "${1:-}" = --register ]; then
    register=1
    shift
fi
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
seconds="${1:-15}"
out="${2:-${TMPDIR:-/tmp}/lba2cc-uwp-run}"
build="${LBA2_UWP_BUILD:-$root/out/build/uwp}"
xbox="${XBOX_SH:-$HOME/code/homelab/ops/xbox.sh}"
full="$(python3 -I "$here/package-name.py")" # from AppxManifest.xml

urlenc() { python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"; }
b64() { urlenc "$(printf '%s' "$1" | base64 | tr -d '\n')"; }
# A running copy holds its .exe open, and the deploy then fails with HTTP 500.
stop() { "$xbox" api DELETE "/api/taskmanager/app?package=$(b64 "$full")" >/dev/null 2>&1 || true; }
local_q="knownfolderid=LocalAppData&packagefullname=$(urlenc "$full")"

ninja -C "$build" >/dev/null
stage="$out/lba2cc"
rm -rf "$stage" && mkdir -p "$stage"
cp "$build/SOURCES/lba2cc.exe" "$here/AppxManifest.xml" "$stage/"
cp -R "$here/Assets" "$stage/"

stop
# Read whole before matching: under pipefail an early-exiting grep -q can fail the pipe.
installed="$("$xbox" apps | cut -f1)"
if [ "$register" = 1 ] || ! grep -qx "$full" <<<"$installed"; then
    "$xbox" deploy "$stage"
else
    "$xbox" api POST "/api/filesystem/apps/file?knownfolderid=DevelopmentFiles&path=$(urlenc '\LooseApps\lba2cc')" \
        -F "f=@$stage/lba2cc.exe" -o /dev/null >/dev/null
    echo "replaced lba2cc.exe"
fi
"$xbox" launch "$full"
sleep "$seconds"
"$xbox" screenshot "$out/screenshot.png"
stop

# LocalState's files, and its folders other than the game data, one level down.
rm -rf "${out:?}/LocalState" && mkdir -p "$out/LocalState"
listing() {
    "$xbox" api GET "/api/filesystem/apps/files?$local_q&path=$(urlenc "$1")" |
        python3 -I -c '
import json, sys
for i in json.load(sys.stdin).get("Items", []):
    print(("d" if i.get("Type", 0) & 16 else "f") + "\t" + i["Name"])'
}
fetch() { # PATH NAME DEST
    "$xbox" api GET "/api/filesystem/apps/file?$local_q&path=$(urlenc "$1")&filename=$(urlenc "$2")" -o "$3" >/dev/null
}
while IFS=$'\t' read -r kind name; do
    if [ "$kind" = f ]; then
        fetch '\LocalState' "$name" "$out/LocalState/$name"
    elif [ "$name" != Game ]; then
        mkdir -p "$out/LocalState/$name"
        while IFS=$'\t' read -r k n; do
            [ "$k" = f ] && fetch "\\LocalState\\$name" "$n" "$out/LocalState/$name/$n"
        done < <(listing "\\LocalState\\$name")
    fi
done < <(listing '\LocalState')

echo "--- $out/LocalState:"
(cd "$out/LocalState" && find . -type f | sort)
echo "--- screenshot: $out/screenshot.png"
