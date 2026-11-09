#!/usr/bin/env bash
# Copy game data into the app's LocalState\Game on an Xbox in Developer Mode, where the UWP
# build looks for it (it passes --game-dir LocalState\Game).
#
#   tools/uwp/xbox-push-data.sh GAME_DIR [SUBFOLDER...]
#
# GAME_DIR is the folder holding the HQR files (a retail install's Common). Only its files
# are copied unless SUBFOLDERs are named (VOX, VIDEO, Music): those add voices, video and
# music, which the silent build does not use yet. Files are overwritten, never deleted.
# The app must have run once since it was registered (tools/uwp/xbox-run.sh does both):
# its LocalState does not exist before, and the portal then accepts uploads and drops them.
set -euo pipefail

src="${1:?usage: tools/uwp/xbox-push-data.sh GAME_DIR [SUBFOLDER...]}"
shift
[ -d "$src" ] || { echo "no folder $src" >&2; exit 1; }
src="$(cd "$src" && pwd)"
xbox="${XBOX_SH:-$HOME/code/homelab/ops/xbox.sh}"
full="LBA2CC_0.0.1.0_x64__bzkng0es5r9z8"

urlenc() { python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"; }
q="knownfolderid=LocalAppData&packagefullname=$(urlenc "$full")"

mkfolder() { # PARENT NAME; creating one that exists is harmless
    "$xbox" api POST "/api/filesystem/apps/folder?$q&path=$(urlenc "$1")&newfoldername=$(urlenc "$2")" -d '' -o /dev/null >/dev/null 2>&1 || true
}
upload() { # LOCAL_DIR REMOTE_PATH: the folder's files, one request per file
    local f n=0
    while IFS= read -r f; do
        XBOX_TIMEOUT=900 "$xbox" api POST "/api/filesystem/apps/file?$q&path=$(urlenc "$2")" -F "f=@$f" -o /dev/null >/dev/null
        n=$((n + 1))
    done < <(find "$1" -maxdepth 1 -type f ! -name '.*' | sort)
    echo "copied $n files to $2"
}

root="$("$xbox" api GET "/api/filesystem/apps/files?$q&path=")"
if ! grep -q '"Name" : "LocalState"' <<<"$root"; then
    echo "$full has no LocalState yet: run it once first (tools/uwp/xbox-run.sh)" >&2
    exit 1
fi
mkfolder '\LocalState' Game
upload "$src" '\LocalState\Game'
for sub in "$@"; do
    [ -d "$src/$sub" ] || { echo "no folder $src/$sub" >&2; exit 1; }
    mkfolder '\LocalState\Game' "$sub"
    upload "$src/$sub" "\\LocalState\\Game\\$sub"
done
