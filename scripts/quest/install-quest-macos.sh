#!/usr/bin/env bash
# Install the prebuilt LBA2 Quest APK and copy the retail game data.
#
# The script downloads Android platform-tools into a temporary directory,
# fetches the latest Quest APK from the fork's GitHub Releases, copies the
# folder containing lba2.hqr to /sdcard/lba2cc/, and installs the APK.
#
# Usage:
#   bash install-quest-macos.sh [options]
#
# Options:
#   --assets <path>   Folder containing lba2.hqr (prompted if omitted)
#   --apk <path>      Use a local APK instead of downloading a release
#   --tag <tag>       Release tag to install (default: latest stable release;
#                     use "latest" for the rolling pre-release)
#   --clean           Uninstall the previous app before installing
#   --no-launch       Do not launch the app after install
#   --keep-tools      Keep the temporary platform-tools and APK
#   --dry-run         Print the actions without touching the headset
#   -h, --help        Show this help
#
# Requirements: a Quest 2/3/Pro with Developer Mode and USB debugging
# enabled, a data-capable USB cable, and your own LBA2 game data.
set -euo pipefail

REPO="simsas-experiments/lba2-classic-quest"
PACKAGE="org.lbalab.lba2cc"
ACTIVITY="org.libsdl.app.SDLActivity"
DEST="/sdcard/lba2cc"
PLATFORM_TOOLS_URL="https://dl.google.com/android/repository/platform-tools-latest-darwin.zip"

ASSETS=""
LOCAL_APK=""
TAG=""
CLEAN=0
LAUNCH=1
KEEP_TOOLS=0
DRY_RUN=0
SERIAL=""
TMP=""

usage() {
    sed -n '2,/^set -euo pipefail/p' "$0" | sed 's/^# \{0,1\}//' | sed '$d'
}

banner() {
    cat <<'EOF'

  _     ____    _    ____
 | |   | __ )  / \  |___ \
 | |   |  _ \ / _ \   __) |
 | |___| |_) / ___ \ / __/
 |_____|____/_/   \_\_____|

  Twinsen's Odyssey on Meta Quest - installer

EOF
}

step() {
    printf '\n==> %s\n' "$*"
}

finished() {
    printf '\n============================================================\n'
    printf ' LBA2 is installed on the Quest.\n'
    printf ' Launch it from the library: Library > Unknown Sources.\n'
    printf '============================================================\n\n'
}

wait_for_exit() {
    # Keep the window open when the script is launched from a file manager.
    # Skip when stdin is not a terminal so pipelines and CI do not block.
    if [ -t 0 ]; then
        printf 'Press any key to exit...'
        read -r -n 1 -s || true
        printf '\n'
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        --assets) ASSETS="${2:-}"; shift 2 ;;
        --apk) LOCAL_APK="${2:-}"; shift 2 ;;
        --tag) TAG="${2:-}"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        --no-launch) LAUNCH=0; shift ;;
        --keep-tools) KEEP_TOOLS=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ "$(uname -s)" != "Darwin" ]; then
    echo "This installer is for macOS. Use the Linux or Windows installer instead." >&2
    exit 1
fi

for tool in curl unzip; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required tool not found: $tool" >&2
        echo "Install it with your package manager and retry." >&2
        exit 1
    fi
done

banner
step "Starting the installer"

github_curl() {
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        curl -fsSL --retry 3 --retry-delay 2 \
            -H "Authorization: Bearer $GITHUB_TOKEN" "$@"
    else
        curl -fsSL --retry 3 --retry-delay 2 "$@"
    fi
}

TMP="$(mktemp -d "${TMPDIR:-/tmp}/lba2-quest.XXXXXX")"
cleanup() {
    if [ -n "$TMP" ] && [ -d "$TMP" ]; then
        if [ "$KEEP_TOOLS" -eq 1 ]; then
            printf '\nKept temporary files in: %s\n' "$TMP"
        else
            rm -rf "$TMP"
        fi
    fi
    wait_for_exit
}
trap cleanup EXIT

step "Downloading Android platform-tools"
curl -fL --retry 3 --retry-delay 2 -o "$TMP/platform-tools.zip" "$PLATFORM_TOOLS_URL"
unzip -q "$TMP/platform-tools.zip" -d "$TMP"
ADB="$TMP/platform-tools/adb"
if [ ! -x "$ADB" ]; then
    echo "adb was not found after extracting platform-tools." >&2
    exit 1
fi
chmod +x "$ADB" 2>/dev/null || true
xattr -dr com.apple.quarantine "$TMP/platform-tools" 2>/dev/null || true

ADB_FILE="$(file "$ADB")"
if [ "$(uname -m)" = "arm64" ] &&
    printf '%s' "$ADB_FILE" | grep -q "x86_64" &&
    ! printf '%s' "$ADB_FILE" | grep -q "arm64"; then
    if ! /usr/bin/arch -x86_64 /usr/bin/true >/dev/null 2>&1; then
        echo "adb is an Intel binary and Rosetta 2 is not installed." >&2
        echo "Install Rosetta with: softwareupdate --install-rosetta --agree-to-license" >&2
        echo "Alternatively, install Android platform-tools with Homebrew." >&2
        exit 1
    fi
fi

if [ -n "$LOCAL_APK" ]; then
    if [ ! -f "$LOCAL_APK" ]; then
        echo "APK not found: $LOCAL_APK" >&2
        exit 1
    fi
    APK_PATH="$LOCAL_APK"
    APK_NAME="$(basename "$APK_PATH")"
else
    if [ -n "$TAG" ]; then
        API_URL="https://api.github.com/repos/$REPO/releases/tags/$TAG"
    else
        API_URL="https://api.github.com/repos/$REPO/releases/latest"
    fi

    step "Reading the Quest release"
    if ! RELEASE_JSON="$(github_curl \
        -H 'Accept: application/vnd.github+json' \
        -H 'User-Agent: lba2-quest-installer' "$API_URL")"; then
        echo "Could not read the GitHub release: $API_URL" >&2
        if [ -z "${GITHUB_TOKEN:-}" ]; then
            echo "If the repository is private, set GITHUB_TOKEN and retry." >&2
        fi
        echo "Check the release name and your network connection." >&2
        exit 1
    fi

    APK_NAME="$(printf '%s\n' "$RELEASE_JSON" |
        grep -oE '"name": *"[^"]*-quest-arm64-v8a\.apk"' |
        head -n1 | sed 's/^"name": *"//; s/"$//')"
    ASSET_API_URL="$(printf '%s\n' "$RELEASE_JSON" |
        grep -oE '"url":"https://api\.github\.com/repos/[^"]*/releases/assets/[0-9]+","id":[0-9]+,"node_id":"[^"]*","name":"[^"]*-quest-arm64-v8a\.apk"' |
        head -n1 |
        grep -oE 'https://api.github.com/repos/[^"]*/releases/assets/[0-9]+')"
    APK_URL="$(printf '%s\n' "$RELEASE_JSON" |
        grep -oE 'https://[^"]*/lba2cc-[^"/]*-quest-[^"/]*\.apk' |
        head -n1 || true)"

    if [ -z "$APK_NAME" ] || { [ -z "$ASSET_API_URL" ] && [ -z "$APK_URL" ]; }; then
        echo "The release does not contain a Quest APK." >&2
        echo "Try --tag latest for the rolling pre-release, or use --apk." >&2
        exit 1
    fi

    APK_PATH="$TMP/$APK_NAME"
    step "Downloading $APK_NAME"
    if [ -n "$ASSET_API_URL" ]; then
        github_curl -H 'Accept: application/octet-stream' \
            -o "$APK_PATH" "$ASSET_API_URL"
    else
        github_curl -o "$APK_PATH" "$APK_URL"
    fi
fi

if [ -z "$ASSETS" ]; then
    printf 'Path to the folder containing lba2.hqr: '
    IFS= read -r ASSETS
fi
case "$ASSETS" in
    "~") ASSETS="$HOME" ;;
    "~"/*) ASSETS="$HOME/${ASSETS#~/}" ;;
esac

if [ ! -d "$ASSETS" ]; then
    echo "Folder not found: $ASSETS" >&2
    exit 1
fi

if [ ! -f "$ASSETS/lba2.hqr" ]; then
    for sub in Common CommonClassic; do
        if [ -f "$ASSETS/$sub/lba2.hqr" ]; then
            ASSETS="$ASSETS/$sub"
            break
        fi
    done
fi

if [ ! -f "$ASSETS/lba2.hqr" ]; then
    echo "lba2.hqr was not found in: $ASSETS" >&2
    echo "Point the script at the folder that contains lba2.hqr." >&2
    exit 1
fi

if [ "$DRY_RUN" -eq 1 ]; then
    echo "[dry-run] adb:    $ADB"
    echo "[dry-run] APK:    $APK_PATH"
    echo "[dry-run] assets: $ASSETS"
    echo "[dry-run] would run: adb shell mkdir -p $DEST"
    echo "[dry-run] would run: adb push $ASSETS/. $DEST/"
    echo "[dry-run] would run: adb install -r $APK_PATH"
    if [ "$LAUNCH" -eq 1 ]; then
        echo "[dry-run] would run: adb shell am start -n $PACKAGE/$ACTIVITY"
    fi
    exit 0
fi

step "Checking for a connected Quest"
"$ADB" start-server >/dev/null 2>&1 || true
DEVICES="$("$ADB" devices 2>/dev/null || true)"
ONLINE="$(printf '%s\n' "$DEVICES" | awk 'NR>1 && $2=="device" {print $1}')"
UNAUTHORIZED="$(printf '%s\n' "$DEVICES" | awk 'NR>1 && $2=="unauthorized" {print $1}')"

if [ -z "$ONLINE" ]; then
    if [ -n "$UNAUTHORIZED" ]; then
        echo "The Quest is connected but not authorized." >&2
        echo "Accept the USB debugging prompt in the headset, then retry." >&2
    else
        cat >&2 <<'EOF'
No Quest found.

Checklist:
  - Developer Mode is enabled on the headset.
  - USB debugging is enabled in the headset developer settings.
  - The USB cable supports data, not just charging.
  - Accept the USB debugging prompt in the headset.
EOF
    fi
    exit 1
fi

DEVICE_COUNT="$(printf '%s\n' "$ONLINE" | wc -l | tr -d ' ')"
if [ "$DEVICE_COUNT" -gt 1 ]; then
    printf 'Multiple devices connected:\n%s\n' "$ONLINE"
    printf 'Enter the serial to use: '
    IFS= read -r SERIAL
    if ! printf '%s\n' "$ONLINE" | grep -qx "$SERIAL"; then
        echo "Unknown serial: $SERIAL" >&2
        exit 1
    fi
else
    SERIAL="$ONLINE"
fi

adb_cmd() {
    "$ADB" -s "$SERIAL" "$@"
}

step "Copying game files to $DEST"
adb_cmd shell mkdir -p "$DEST"
adb_cmd push "$ASSETS/." "$DEST/"

if [ "$CLEAN" -eq 1 ]; then
    echo "Removing the previous install (app-local saves may be lost)..."
    adb_cmd uninstall "$PACKAGE" >/dev/null 2>&1 || true
fi

install_once() {
    adb_cmd install -r "$APK_PATH" >"$TMP/install.log" 2>&1
}

step "Installing $APK_NAME"
if ! install_once; then
    INSTALL_OUTPUT="$(cat "$TMP/install.log")"
    printf '%s\n' "$INSTALL_OUTPUT"
    case "$INSTALL_OUTPUT" in
        *INSTALL_FAILED_UPDATE_INCOMPATIBLE*|*"signatures do not match"*)
            if [ "$CLEAN" -ne 1 ]; then
                printf '\nThe existing app was signed with a different debug key.\n'
                printf 'Uninstall %s and reinstall? App-local saves may be lost. [y/N] ' "$PACKAGE"
                IFS= read -r ANSWER
                case "$ANSWER" in
                    y|Y|yes|YES)
                        adb_cmd uninstall "$PACKAGE" >/dev/null 2>&1 || true
                        if ! install_once; then
                            cat "$TMP/install.log"
                            exit 1
                        fi
                        ;;
                    *)
                        echo "Re-run with --clean to replace the existing install." >&2
                        exit 1
                        ;;
                esac
            else
                exit 1
            fi
            ;;
        *)
            exit 1
            ;;
    esac
fi

echo "Installed $APK_NAME."

if [ "$LAUNCH" -eq 1 ]; then
    if adb_cmd shell am start -n "$PACKAGE/$ACTIVITY" >/dev/null 2>&1; then
        echo "Launched $PACKAGE."
    else
        echo "Installed, but the app could not be launched automatically. Open it from the Quest library."
    fi
fi

finished
