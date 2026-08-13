#!/usr/bin/env bash
# Post-release smoke test for the published release artifacts.
#
# Downloads what a GitHub Release actually serves and checks each artifact
# as far as this host allows. The Linux tarballs and AppImages are extracted
# and run in a clean debian:stable-slim container with no SDL3 / libsmacker
# / X11 deps installed, which is the signal CI doesn't cover: the artifact
# GitHub *serves* (post-upload, post-download) runs on a fresh system, with
# the executable bit preserved and the static linking claim holding.
#
# Four metadata checks ride along, because each failure mode is silent
# until a user hits it:
#
#   * `--version` agrees with the version in the filename, so a
#     mislabelled artifact can't reach the Releases page.
#   * The AppImage's baked-in self-update channel matches the release it
#     was attached to. This one needs no container, so it covers the
#     cross-arch AppImages the run check has to skip.
#   * The AppImage carries its AppStream metainfo, which decides how the
#     app presents itself in software centres and the AppImageHub catalog.
#   * The .zsync control file still describes the AppImage it was published
#     with, so self-update works for people who already have the file.
#
# The Windows ZIP is checked too. Under WSL a Windows .exe launched from
# Linux runs as a Windows process, so the shipped binary answers `--version`
# with no wine and no VM. Off WSL that check reports SKIP and the PE
# metadata is still read, which needs nothing but grep.
#
# The macOS DMG can't be executed without a Mac, but everything it declares
# can be read here: the disk image opens with 7z, the bundle's Info.plist is
# what Finder and Gatekeeper read, and the Mach-O's load commands say which
# libraries it expects to find on the target machine.
#
# The Android APKs are read with the SDK build-tools, which check the three
# things an install fails on: the manifest, the signature, and 16 KB page
# safety. Missing build-tools reports SKIP.
#
# Usage:
#   scripts/dev/verify-release.sh [<tag>]
#
# Default tag: `latest` (the rolling pre-release). Pass a versioned tag
# (e.g. `v0.9.0`) as a pre-publicize gate before announcing a release.
#
# Exit codes:
#   0  everything checked passed
#   1  a check failed
#   2  the metadata checks passed but nothing was run in a clean container
#      (no container runtime), so the result is not a release gate
#
# Requirements:
#   - gh (authenticated), tar
#   - docker, for the clean-system claim only. Without a reachable daemon
#     both metadata checks still run: the update channel is read from the
#     file, and `--version` is taken by running same-arch artifacts on this
#     host instead. Cross-arch artifacts are then not run at all. Exit 2
#     either way, because "runs with none of its build deps installed" is
#     what only a container can show.
#   - aarch64 leg auto-registers qemu-user-static binfmt via
#     tonistiigi/binfmt if not already set up
set -euo pipefail

TAG="${1:-latest}"

# Self-update channel each AppImage on this release should carry. In
# gh-releases-zsync the tag field is a reserved keyword rather than a tag
# name: `latest` means newest non-prerelease, `latest-pre` newest
# prerelease. The rolling release is tagged `latest` *and* flagged
# prerelease, so a rolling AppImage left on `latest` resolves to the newest
# stable tag and overwrites itself with it on first run, keeping its own
# rolling filename, so its banner then reports an older version than the
# name promises. See docs/RELEASING.md "Rolling latest pre-release".
case "$TAG" in
    latest) EXPECT_CHANNEL="latest-pre" ;;
    *)      EXPECT_CHANNEL="latest" ;;
esac

for tool in gh tar; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "verify-release: required tool not found: $tool" >&2
        exit 1
    fi
done

# Docker is checked by reachability, not by presence on PATH. Docker Desktop
# drops a `docker` shim into WSL distros that resolves fine and then fails at
# exec time, so `command -v` answers yes on exactly the hosts where nothing
# can run — and every run check then reports FAIL with the shim's help text
# folded into the row, which reads as a broken release rather than a broken
# workstation.
#
# Without a daemon the metadata checks still carry their full weight, so the
# run proceeds; the run checks become SKIP rows and the exit code says so.
HOST_ARCH="$(uname -m)"
DOCKER_OK=1
SKIPPED_NO_DOCKER=0
if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    DOCKER_OK=0
    echo "verify-release: no reachable container runtime — the clean-system run" >&2
    echo "                checks will be skipped; metadata checks still run." >&2
fi

# Resolve the repo for gh from the script's location, not the caller's
# cwd. The script downloads into a mktemp dir, so gh would otherwise
# lose its git context and fail with "not a git repository".
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
REPO_SLUG="$(cd "$REPO_ROOT" && gh repo view --json nameWithOwner -q .nameWithOwner)"

WORK_DIR="$(mktemp -d -t lba2-verify-release.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "[verify-release] tag:     $TAG"
echo "[verify-release] workdir: $WORK_DIR"

cd "$WORK_DIR"

echo "[verify-release] downloading artifacts..."
gh release download "$TAG" \
    --repo "$REPO_SLUG" \
    --pattern 'lba2cc-*-linux-*.tar.gz' \
    --pattern 'lba2cc-*-anylinux-*.AppImage' \
    --pattern 'lba2cc-*-AppImage-*.AppImage' \
    --pattern 'lba2cc-*.AppImage.zsync' \
    --pattern 'lba2cc-*-windows-*.zip' \
    --pattern 'lba2cc-*-macos-*.dmg' \
    --pattern 'lba2cc-*-android-*.apk' \
    --dir . 2>&1 | tail -5 || true

# Collect what actually landed — release artifact naming has shifted
# between AppImage flavors (anylinux-* vs AppImage-*), and an arch leg
# may have failed in the rolling release, so don't assume all 4 exist.
shopt -s nullglob
TARBALLS=( lba2cc-*-linux-*.tar.gz )
APPIMAGES=( lba2cc-*-*linux*-*.AppImage lba2cc-*-AppImage-*.AppImage )
WIN_ZIPS=( lba2cc-*-windows-*.zip )
MAC_DMGS=( lba2cc-*-macos-*.dmg )
APKS=( lba2cc-*-android-*.apk )
shopt -u nullglob

if [[ ${#TARBALLS[@]} -eq 0 && ${#APPIMAGES[@]} -eq 0 ]]; then
    echo "[verify-release] no Linux artifacts found on release $TAG" >&2
    exit 1
fi

# aarch64 binfmt — register qemu-user-static if not already, so
# --platform linux/arm64 containers can exec. Idempotent.
need_aarch64=0
for f in "${TARBALLS[@]}" "${APPIMAGES[@]}"; do
    [[ "$f" == *aarch64* ]] && need_aarch64=1 && break
done
if (( need_aarch64 && DOCKER_OK )); then
    if ! docker run --rm --platform linux/arm64 debian:stable-slim \
            true >/dev/null 2>&1; then
        echo "[verify-release] registering qemu-user-static binfmt..."
        # Unguarded, this aborts the whole run under `set -e` on any host
        # that can't register binfmt (no privileged containers, rootless
        # podman, no network for the binfmt image), taking the x86_64
        # results and the whole summary table down with it. Same reasoning
        # as run_check: record what fails, keep checking the rest.
        if ! docker run --privileged --rm tonistiigi/binfmt --install arm64 \
                >/dev/null 2>&1; then
            echo "[verify-release] binfmt registration failed; aarch64 legs will be reported as failures" >&2
        fi
    fi
fi

# Run a single artifact through extraction + clean-Docker --version,
# print one row of the result table.
PASS=0
FAIL=0
declare -a RESULTS

# One row of the result table. Every check ends in exactly one of these, so
# the tally and the printed table can't drift apart.
pass_row() { RESULTS+=( "PASS  $1  $2" ); PASS=$(( PASS + 1 )); }
fail_row() { RESULTS+=( "FAIL  $1  $2" ); FAIL=$(( FAIL + 1 )); }
skip_row() { RESULTS+=( "SKIP  $1  $2" ); }

# Version embedded in an artifact filename, which is always
# lba2cc-<version>-<platform>-<arch>.<ext>. Strip the prefix and the two
# trailing fields. <version> itself contains dashes (`0.13.0-dev`), so peel
# from the right rather than splitting on the first one.
version_from_name() {
    local stem="${1%.tar.gz}"
    stem="${stem%.AppImage}"
    stem="${stem%.zip}"
    stem="${stem%.dmg}"
    stem="${stem%.apk}"
    stem="${stem#lba2cc-}"
    # Android ABI names carry their own dashes (arm64-v8a, armeabi-v7a), so
    # peel that suffix off by name instead of by counting fields.
    case "$stem" in
        *-android-*) printf '%s' "${stem%%-android-*}"; return ;;
    esac
    stem="${stem%-*}"
    printf '%s' "${stem%-*}"
}

# The update information is a plain string in the AppImage's .upd_info ELF
# section. grep it out rather than making binutils a host requirement.
check_update_channel() {
    local label="$1" aimg="$2"
    local upd channel
    upd=$( grep -a -o -m1 'gh-releases-zsync|[^|]*|[^|]*|[^|]*|' "$aimg" || true )
    if [[ -z "$upd" ]]; then
        fail_row "$label" "carries no gh-releases-zsync update information"
        return
    fi
    channel=$( printf '%s' "$upd" | cut -d'|' -f4 )
    if [[ "$channel" == "$EXPECT_CHANNEL" ]]; then
        pass_row "$label" "update-channel=$channel"
    else
        fail_row "$label" "update-channel=$channel, expected $EXPECT_CHANNEL on tag $TAG"
    fi
}

# Each AppImage is published with a .zsync control file, and that file is
# what an updater fetches first: a plain-text header naming the target, its
# length and its SHA-1, then the block sums it diffs against. If the header
# drifts from the artifact actually attached to the release (an AppImage
# rebuilt and re-uploaded over an older .zsync, or an upload that half
# failed), self-update breaks for everyone who already has the file, and
# nothing else here would notice: the AppImage still runs and still names
# the right update channel. Only comparing the two files catches it.
check_zsync() {
    local label="$1" zs="$2" aimg="$3"
    local line key val want_name="" want_len="" want_sha="" got_len got_sha

    if [[ ! -f "$zs" ]]; then
        skip_row "$label" "no .zsync published alongside this AppImage"
        return
    fi
    # The header is text terminated by a blank line, and the block sums
    # after it are binary, so the loop breaks on that line and never reads
    # into them.
    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -z "$line" ]] && break
        key="${line%%: *}"
        val="${line#*: }"
        case "$key" in
            Filename) want_name="$val" ;;
            Length)   want_len="$val"  ;;
            SHA-1)    want_sha="$val"  ;;
        esac
    done < "$zs"

    got_len=$( wc -c < "$aimg" | tr -d ' ' )
    got_sha=$( sha1sum "$aimg" | cut -d' ' -f1 )

    if [[ "$want_name" != "${aimg##*/}" ]]; then
        fail_row "$label" "zsync names $want_name, but it sits beside ${aimg##*/}"
    elif [[ "$want_len" != "$got_len" ]]; then
        fail_row "$label" "zsync length=$want_len, artifact=$got_len"
    elif [[ "$want_sha" != "$got_sha" ]]; then
        fail_row "$label" "zsync SHA-1=$want_sha, artifact=$got_sha"
    else
        pass_row "$label" "zsync matches artifact (sha1=${got_sha:0:12})"
    fi
}

run_check() {
    local label="$1" platform="$2" mount_mode="$3" cmd="$4" expected="$5"
    local out rc version note=""
    local want_arch="x86_64"
    [[ "$platform" == "linux/arm64" ]] && want_arch="aarch64"

    if (( ! DOCKER_OK )) && [[ "$want_arch" != "$HOST_ARCH" ]]; then
        skip_row "$label" "no container runtime and cross-arch: not run at all"
        SKIPPED_NO_DOCKER=$(( SKIPPED_NO_DOCKER + 1 ))
        return
    fi

    if (( DOCKER_OK )); then
        # Guard against set -e propagating from $() when docker exits
        # non-zero — we want to record the failure as a FAIL row, not abort
        # the whole script and leave the rest of the artifacts unchecked.
        if out=$( docker run --rm --platform "$platform" \
            -v "$WORK_DIR:/test:$mount_mode" debian:stable-slim \
            sh -c "$cmd" 2>&1 ); then
            rc=0
        else
            rc=$?
        fi
    else
        # No daemon, but the artifact is this host's architecture, so the
        # version-vs-filename half of the check is still reachable: run it
        # here instead. What is lost is the clean-system claim, since this
        # box has the build deps installed, so the row says so and the run
        # still exits 2. A mislabelled artifact is caught either way, which
        # is the failure that otherwise reaches the Releases page unseen.
        SKIPPED_NO_DOCKER=$(( SKIPPED_NO_DOCKER + 1 ))
        note="  (host run; clean-system NOT verified)"
        if out=$( sh -c "${cmd//\/test\//$WORK_DIR/}" 2>&1 ); then
            rc=0
        else
            rc=$?
        fi
    fi
    # --version may be preceded by stderr warnings (e.g. AppRun's
    # "Cannot find CA Certificates" notice) folded in via 2>&1. The
    # real version is the last non-empty line.
    version=$( echo "$out" | awk 'NF{last=$0} END{print last}' )
    if [[ $rc -ne 0 || -z "$version" ]]; then
        fail_row "$label" "rc=$rc out=$out"
    elif [[ "$version" != "$expected" ]]; then
        fail_row "$label" "--version=$version but the filename says $expected"
    else
        pass_row "$label" "--version=$version$note"
    fi
}

for tgz in "${TARBALLS[@]}"; do
    stem="${tgz%.tar.gz}"
    case "$tgz" in
        *aarch64*) platform="linux/arm64" ;;
        *)         platform="linux/amd64" ;;
    esac
    # Extract on the host (tar is arch-agnostic), then exec in-arch
    # under the matching container. Read-only mount is fine — the
    # binary doesn't write to its directory.
    tar xzf "$tgz"
    run_check \
        "tarball $stem" \
        "$platform" \
        "ro" \
        "/test/$stem/lba2cc --version" \
        "$( version_from_name "$tgz" )"
done

# AppImage verification has a cross-arch limitation: the AppImage type-2
# runtime stub uses syscalls / binary patterns that qemu-user's binfmt
# handler doesn't translate reliably, so an aarch64 AppImage run inside
# an arm64-emulated container exits with "Exec format error" even
# though the file is valid aarch64 ELF (the tarball binary's plain glibc
# code works in the same container, which is what isolates the cause
# to the AppImage runtime stub specifically). Skip cross-arch AppImages
# and verify only AppImages whose arch matches the host.
for aimg in "${APPIMAGES[@]}"; do
    stem="${aimg%.AppImage}"
    case "$aimg" in
        *aarch64*) aimg_arch="aarch64"; platform="linux/arm64" ;;
        *x86_64*)  aimg_arch="x86_64";  platform="linux/amd64" ;;
        *)         aimg_arch="unknown"; platform="linux/amd64" ;;
    esac
    # Both read the files directly, so they run for every AppImage including
    # the cross-arch ones skipped below.
    check_update_channel "appimage $stem" "$WORK_DIR/$aimg"
    check_zsync "appimage $stem" "$WORK_DIR/$aimg.zsync" "$WORK_DIR/$aimg"
    if [[ "$aimg_arch" != "$HOST_ARCH" ]]; then
        skip_row "appimage $stem" "cross-arch AppImage (host=$HOST_ARCH, image=$aimg_arch) — qemu-user can't run AppImage runtime stub"
        continue
    fi
    # Native arch — extract on the host (AppImage runtime stub runs
    # directly, no qemu needed), then exec AppRun inside a slim
    # container to confirm portability. Each AppImage gets its own
    # sandbox dir so they don't collide.
    extract_dir="appimage-$stem"
    mkdir -p "$WORK_DIR/$extract_dir"
    chmod +x "$aimg"
    if ! ( cd "$WORK_DIR/$extract_dir" && "$WORK_DIR/$aimg" --appimage-extract >/dev/null 2>&1 ); then
        fail_row "appimage $stem" "--appimage-extract failed on host"
        continue
    fi
    # AppStream metainfo, at the path AppImageHub and desktop-integration
    # tools read. Nothing at runtime touches it, and CI validates the
    # generated file rather than the assembled AppDir, so a packaging change
    # that stopped copying it in would leave every other check green.
    metainfo=( "$WORK_DIR/$extract_dir"/squashfs-root/usr/share/metainfo/*.metainfo.xml )
    if [[ -f "${metainfo[0]}" ]]; then
        pass_row "appimage $stem" "metainfo=${metainfo[0]##*/}"
    else
        fail_row "appimage $stem" "no usr/share/metainfo/*.metainfo.xml in the AppDir"
    fi

    run_check \
        "appimage $stem" \
        "$platform" \
        "ro" \
        "/test/$extract_dir/squashfs-root/AppRun --version" \
        "$( version_from_name "$aimg" )"
done

# The Windows ZIP. Two signals, and only the second needs a Windows.
#
# VERSIONINFO is a resource block inside the PE holding the strings
# Explorer shows under Properties > Details. It is UTF-16LE, so dropping the
# NUL bytes turns it back into greppable text, on the same reasoning as the
# AppImage .upd_info check: no binutils requirement for one string.
#
# Then the binary is run. WSL registers a binfmt handler for PE images, so a
# Windows .exe launched from a Linux path executes as a real Windows
# process, which makes this the cheapest platform here to verify rather than
# the most expensive. It also exercises the claim that the ZIP is complete,
# since a missing runtime DLL shows up as a launch failure. Off WSL, or with
# the handler shadowed, the row says so and the metadata check still stands.
for zip in "${WIN_ZIPS[@]}"; do
    stem="${zip%.zip}"
    label="windows $stem"
    expected="$( version_from_name "$zip" )"

    if ! command -v unzip >/dev/null 2>&1; then
        skip_row "$label" "unzip not installed, ZIP not opened"
        continue
    fi
    extract_dir="windows-$stem"
    mkdir -p "$WORK_DIR/$extract_dir"
    if ! unzip -q -o "$zip" -d "$WORK_DIR/$extract_dir" 2>/dev/null; then
        fail_row "$label" "unzip failed"
        continue
    fi
    # -print -quit rather than `| head -1`: under pipefail, head closing the
    # pipe early can make the whole substitution fail, and under set -e that
    # ends the run.
    exe=$( find "$WORK_DIR/$extract_dir" -name 'lba2cc.exe' -type f -print -quit )
    if [[ -z "$exe" ]]; then
        fail_row "$label" "no lba2cc.exe in the ZIP"
        continue
    fi

    # Drop the NULs once, into a file. Grepping a file rather than a pipe
    # matters here: `grep -q` stops at the first match, which closes the pipe
    # under it, and pipefail then reports the whole pipeline as failed.
    pe_text="$WORK_DIR/$extract_dir/pe-strings.txt"
    tr -d '\0' < "$exe" > "$pe_text"

    # Anchored on the key name, so this can't pass on a version that happens
    # to appear elsewhere in the image. The trailing byte after the value is
    # the next record's length field, so match a prefix rather than equality.
    if ! grep -a -q -F "FileVersion$expected" "$pe_text"; then
        found=$( grep -a -o -m1 'FileVersion[0-9][ -~]\{0,32\}' "$pe_text" || true )
        fail_row "$label" "VERSIONINFO ${found:-carries no FileVersion}, filename says $expected"
    elif ! grep -a -q 'FileDescription[ -~]\{4,\}' "$pe_text"; then
        fail_row "$label" "VERSIONINFO has FileVersion=$expected but no FileDescription"
    else
        pass_row "$label" "VERSIONINFO FileVersion=$expected, FileDescription set"
    fi

    chmod +x "$exe"
    win_run=( "$exe" --version )
    # A release binary that hangs must not hang the verification.
    command -v timeout >/dev/null 2>&1 && win_run=( timeout 60 "${win_run[@]}" )
    if out=$( "${win_run[@]}" 2>&1 ); then rc=0; else rc=$?; fi
    out=$( printf '%s' "$out" | tr -d '\r' )
    version=$( printf '%s\n' "$out" | awk 'NF{last=$0} END{print last}' )

    # Tell "this host can't launch PE binaries" apart from "the binary is
    # broken". The first is a property of the workstation and has to read as
    # SKIP; the second is exactly what this script exists to catch, so
    # anything that isn't a recognised handler failure counts as a FAIL.
    if [[ $rc -eq 126 ]] || [[ "$out" == *"Exec format error"* ]] \
       || [[ "$out" == *"run-detectors"* ]] || [[ "$out" == *"binfmt"* ]]; then
        # First line only, and clipped: the handler's complaint names the
        # full path, which says nothing and swamps the row.
        reason="${out%%$'\n'*}"
        skip_row "$label" "host cannot launch PE binaries: ${reason:0:60}"
    elif [[ $rc -ne 0 || -z "$version" ]]; then
        fail_row "$label" "rc=$rc out=$out"
    elif [[ "$version" != "$expected" ]]; then
        fail_row "$label" "--version=$version but the filename says $expected"
    else
        pass_row "$label" "--version=$version (ran as a Windows process)"
    fi
done

# The macOS DMG. Nothing here can execute it, so this reads what it
# declares about itself, which is most of what a packaging mistake breaks.
#
# 7z opens the disk image with no loopback mount and no root. The Info.plist
# inside is the file Finder, Launch Services and Gatekeeper read, so a wrong
# version or an empty copyright field is visible to every macOS user and to
# nobody else. The Mach-O's load commands then name every library the binary
# expects to find at runtime: anything outside /usr/lib and /System/Library
# is something the .app would have to carry, so an empty list is how the
# "SDL3 is linked statically" claim gets checked without a Mac.
for dmg in "${MAC_DMGS[@]}"; do
    stem="${dmg%.dmg}"
    label="macos $stem"
    expected="$( version_from_name "$dmg" )"
    case "$dmg" in
        *arm64*)  want_arch="arm64" ;;
        *x86_64*) want_arch="x86_64" ;;
        *)        want_arch="" ;;
    esac

    sevenzip=""
    for candidate in 7z 7zz 7za; do
        if command -v "$candidate" >/dev/null 2>&1; then
            sevenzip="$candidate"
            break
        fi
    done
    if [[ -z "$sevenzip" ]]; then
        skip_row "$label" "no 7z on this host, disk image not opened"
        continue
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        skip_row "$label" "no python3 on this host, Info.plist not read"
        continue
    fi

    extract_dir="macos-$stem"
    mkdir -p "$WORK_DIR/$extract_dir"
    if ! "$sevenzip" x -y -o"$WORK_DIR/$extract_dir" "$dmg" >/dev/null 2>&1; then
        fail_row "$label" "$sevenzip could not open the disk image"
        continue
    fi
    plist=$( find "$WORK_DIR/$extract_dir" -path '*/Contents/Info.plist' -print -quit )
    if [[ -z "$plist" ]]; then
        fail_row "$label" "no .app bundle with a Contents/Info.plist in the disk image"
        continue
    fi

    # plistlib reads both the XML and the binary plist format, and four bytes
    # of the Mach-O header give the architecture, so the bundle answers for
    # itself without a Mach-O aware objdump being installed.
    mac_version=""; mac_id=""; mac_copyright=""; mac_arch=""; mac_exec=""
    while IFS=$'\t' read -r key value; do
        case "$key" in
            version)   mac_version="$value"   ;;
            id)        mac_id="$value"        ;;
            copyright) mac_copyright="$value" ;;
            arch)      mac_arch="$value"      ;;
            exec)      mac_exec="$value"      ;;
        esac
    done < <(python3 - "$plist" <<'PY'
import os
import plistlib
import struct
import sys

plist_path = sys.argv[1]
with open(plist_path, "rb") as handle:
    info = plistlib.load(handle)


def emit(key, value):
    print(f"{key}\t{value}")


emit("version", info.get("CFBundleShortVersionString", ""))
emit("id", info.get("CFBundleIdentifier", ""))
emit("copyright", info.get("NSHumanReadableCopyright", ""))

executable = os.path.join(os.path.dirname(plist_path), "MacOS",
                          info.get("CFBundleExecutable", ""))
if not os.path.isfile(executable):
    emit("exec", "")
    sys.exit(0)
emit("exec", executable)

with open(executable, "rb") as handle:
    header = handle.read(8)
magic = struct.unpack("<I", header[:4])[0]
if magic in (0xFEEDFACF, 0xFEEDFACE):
    cpu = struct.unpack("<I", header[4:8])[0]
elif magic in (0xCFFAEDFE, 0xCEFAEDFE):
    cpu = struct.unpack(">I", header[4:8])[0]
elif magic in (0xBEBAFECA, 0xCAFEBABE):
    cpu = "universal"          # several slices, so no single cputype
else:
    cpu = "not a Mach-O"
emit("arch", {0x0100000C: "arm64", 0x01000007: "x86_64"}.get(cpu, cpu))
PY
    )

    if [[ -z "$mac_exec" ]]; then
        fail_row "$label" "Info.plist names no executable that exists in the bundle"
        continue
    fi
    if [[ "$mac_version" != "$expected" ]]; then
        fail_row "$label" "Info.plist says $mac_version, the filename says $expected"
    elif [[ -z "$mac_id" ]]; then
        fail_row "$label" "bundle carries no CFBundleIdentifier"
    elif [[ "$mac_copyright" != *Copyright* ]]; then
        fail_row "$label" "NSHumanReadableCopyright is not a copyright line: ${mac_copyright:-empty}"
    else
        pass_row "$label" "$mac_id $mac_version, copyright set"
    fi

    if [[ -n "$want_arch" && "$mac_arch" != "$want_arch" ]]; then
        fail_row "$label" "Mach-O is $mac_arch, the filename says $want_arch"
    else
        pass_row "$label" "Mach-O arch=$mac_arch"
    fi

    otool=""
    for candidate in llvm-otool otool; do
        if command -v "$candidate" >/dev/null 2>&1; then
            otool="$candidate"
            break
        fi
    done
    if [[ -z "$otool" ]]; then
        # Packaged per LLVM release rather than on PATH on most distros.
        otool=$( ls -d /usr/lib/llvm-*/bin/llvm-otool 2>/dev/null | sort -V | tail -1 || true )
    fi
    if [[ -z "$otool" ]]; then
        skip_row "$label" "no llvm-otool on this host, linked libraries not read"
    else
        outside=$( "$otool" -L "$mac_exec" 2>/dev/null \
            | awk 'NR > 1 { print $1 }' \
            | grep -v -E '^(/usr/lib/|/System/Library/)' || true )
        if [[ -n "$outside" ]]; then
            fail_row "$label" "links libraries the bundle does not carry: ${outside//$'\n'/ }"
        else
            pass_row "$label" "every linked library is a system one"
        fi
    fi
done

# The Android APKs. An APK can't be run here either, but the three things
# that stop one installing or loading are all readable: the manifest, the
# signature, and 16 KB page alignment.
#
# The tools ship with the SDK rather than the distro, and an SDK install
# doesn't put them on PATH, so they are resolved from ANDROID_HOME (or the
# usual install location) instead of being required.
android_sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
android_build_tools=$( ls -d "$android_sdk"/build-tools/*/ 2>/dev/null | sort -V | tail -1 || true )
android_ndk="${ANDROID_NDK:-}"
if [[ -z "$android_ndk" ]]; then
    android_ndk=$( ls -d "$android_sdk"/ndk/*/ 2>/dev/null | sort -V | tail -1 || true )
fi
# apksigner is a JVM tool. Prefer whatever the caller has set up, then the
# JDK an SDK install usually sits next to.
android_java_home="${JAVA_HOME:-}"
if [[ -z "$android_java_home" ]] && command -v java >/dev/null 2>&1; then
    android_java_home=$( dirname "$( dirname "$( command -v java )" )" )
fi
if [[ -z "$android_java_home" ]]; then
    android_java_home=$( ls -d "$android_sdk"/../jbr "$HOME"/Android/jdk-*/ 2>/dev/null | sort -V | tail -1 || true )
fi

for apk in "${APKS[@]}"; do
    stem="${apk%.apk}"
    label="android $stem"
    expected="$( version_from_name "$apk" )"

    if [[ -z "$android_build_tools" ]]; then
        skip_row "$label" "no SDK build-tools under $android_sdk, APK not read"
        continue
    fi
    aapt2="${android_build_tools}aapt2"
    apksigner="${android_build_tools}apksigner"

    if [[ ! -x "$aapt2" ]]; then
        skip_row "$label" "no aapt2 in $android_build_tools, manifest not read"
    else
        badging=$( "$aapt2" dump badging "$apk" 2>/dev/null || true )
        package_line=$( printf '%s\n' "$badging" | grep -m1 '^package:' || true )
        apk_version=$( printf '%s' "$package_line" | sed -n "s/.*versionName='\([^']*\)'.*/\1/p" )
        apk_code=$( printf '%s' "$package_line" | sed -n "s/.*versionCode='\([^']*\)'.*/\1/p" )
        apk_abi=$( printf '%s\n' "$badging" | sed -n "s/^native-code: '\([^']*\)'.*/\1/p" )
        if [[ -z "$package_line" ]]; then
            fail_row "$label" "aapt2 could not read the manifest"
        elif [[ "$apk_version" != "$expected" ]]; then
            fail_row "$label" "manifest versionName=$apk_version, the filename says $expected"
        elif [[ -n "$apk_abi" && "$apk" != *"$apk_abi"* ]]; then
            fail_row "$label" "manifest native-code=$apk_abi, which the filename does not name"
        else
            pass_row "$label" "versionName=$apk_version versionCode=$apk_code abi=$apk_abi"
        fi
    fi

    if [[ ! -x "$apksigner" ]]; then
        skip_row "$label" "no apksigner in $android_build_tools, signature not checked"
    elif [[ -z "$android_java_home" ]]; then
        skip_row "$label" "no JVM found for apksigner, signature not checked"
    else
        if signing=$( JAVA_HOME="$android_java_home" "$apksigner" verify --verbose "$apk" 2>&1 ); then
            schemes=""
            [[ "$signing" == *"(JAR signing): true"* ]] && schemes="v1"
            [[ "$signing" == *"Scheme v2): true"* ]] && schemes="${schemes:+$schemes+}v2"
            [[ "$signing" == *"Scheme v3): true"* ]] && schemes="${schemes:+$schemes+}v3"
            # v1 alone is refused by every Android since 11, so a release
            # that lost its v2 signature installs nowhere current.
            if [[ "$schemes" == *v2* || "$schemes" == *v3* ]]; then
                pass_row "$label" "signature verifies ($schemes)"
            else
                fail_row "$label" "signature verifies with ${schemes:-no known scheme}, needs v2 or better"
            fi
        else
            fail_row "$label" "signature does not verify: ${signing%%$'\n'*}"
        fi
    fi

    # 16 KB pages are an arm64 concern (Android 15+). A 32-bit ABI legitimately
    # aligns to 4 KB, so running the check there would report a failure that
    # isn't one.
    if [[ "$apk" != *arm64* ]]; then
        skip_row "$label" "16 KB page check does not apply to a 32-bit ABI"
    elif [[ -z "$android_ndk" ]]; then
        skip_row "$label" "no NDK under $android_sdk, 16 KB page safety not checked"
    elif ANDROID_NDK="$android_ndk" ANDROID_HOME="$android_sdk" \
         bash "$SCRIPT_DIR/check-16k-align.sh" "$apk" >/dev/null 2>&1; then
        pass_row "$label" "16 KB-safe"
    else
        fail_row "$label" "not 16 KB-safe, see scripts/dev/check-16k-align.sh $apk"
    fi
done

echo
echo "[verify-release] results for tag $TAG:"
printf '  %s\n' "${RESULTS[@]}"
echo
SKIP=$( printf '%s\n' "${RESULTS[@]}" | grep -c '^SKIP' || true )
echo "[verify-release] $PASS passed, $FAIL failed, $SKIP skipped"

if (( FAIL > 0 )); then
    exit 1
fi

# "Could not check" must not exit like "checked and fine". The run checks are
# the only thing here that proves a served artifact executes on a system with
# none of its build deps; a gate that quietly drops them while returning 0 is
# worse than the FAIL rows this replaced. Distinct code so a caller that only
# wants the metadata checks can choose to ignore it.
if (( SKIPPED_NO_DOCKER > 0 )); then
    echo "[verify-release] $SKIPPED_NO_DOCKER artifact(s) not run in a clean container: no container runtime."
    echo "[verify-release] update channel and version-vs-filename were checked;"
    echo "[verify-release] this run did NOT verify the artifacts"
    echo "[verify-release] execute on a clean system. Not a release gate as it stands."
    exit 2
fi
