#!/usr/bin/env bash
# Post-release smoke test for the Linux release artifacts.
#
# Downloads the Linux tarballs and AppImages attached to a GitHub
# Release, extracts them, and runs each binary in a clean
# debian:stable-slim container with no SDL3 / libsmacker / X11 deps
# installed. Verifies the unique signal CI doesn't cover: the artifact
# GitHub *serves* (post-upload, post-download) actually runs on a fresh
# system, with the executable bit preserved and the static linking
# claim holding.
#
# Windows ZIPs and macOS DMGs aren't checked — running them on a Linux
# host needs wine / qemu-system-x86 / a Mac, which is more machinery
# than the marginal signal justifies. CI already builds and packages
# both.
#
# Usage:
#   scripts/dev/verify-release.sh [<tag>]
#
# Default tag: `latest` (the rolling pre-release). Pass a versioned tag
# (e.g. `v0.9.0`) as a pre-publicize gate before announcing a release.
#
# Requirements:
#   - gh (authenticated)
#   - docker
#   - aarch64 leg auto-registers qemu-user-static binfmt via
#     tonistiigi/binfmt if not already set up
set -euo pipefail

TAG="${1:-latest}"

for tool in gh docker tar; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "verify-release: required tool not found: $tool" >&2
        exit 1
    fi
done

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

echo "[verify-release] downloading Linux artifacts..."
gh release download "$TAG" \
    --repo "$REPO_SLUG" \
    --pattern 'lba2cc-*-linux-*.tar.gz' \
    --pattern 'lba2cc-*-anylinux-*.AppImage' \
    --pattern 'lba2cc-*-AppImage-*.AppImage' \
    --dir . 2>&1 | tail -5 || true

# Collect what actually landed — release artifact naming has shifted
# between AppImage flavors (anylinux-* vs AppImage-*), and an arch leg
# may have failed in the rolling release, so don't assume all 4 exist.
shopt -s nullglob
TARBALLS=( lba2cc-*-linux-*.tar.gz )
APPIMAGES=( lba2cc-*-*linux*-*.AppImage lba2cc-*-AppImage-*.AppImage )
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
if (( need_aarch64 )); then
    if ! docker run --rm --platform linux/arm64 debian:stable-slim \
            true >/dev/null 2>&1; then
        echo "[verify-release] registering qemu-user-static binfmt..."
        docker run --privileged --rm tonistiigi/binfmt --install arm64 \
            >/dev/null 2>&1
    fi
fi

# Run a single artifact through extraction + clean-Docker --version,
# print one row of the result table.
PASS=0
FAIL=0
declare -a RESULTS

run_check() {
    local label="$1" platform="$2" mount_mode="$3" cmd="$4"
    local out rc version
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
    # --version may be preceded by stderr warnings (e.g. AppRun's
    # "Cannot find CA Certificates" notice) folded in via 2>&1. The
    # real version is the last non-empty line.
    version=$( echo "$out" | awk 'NF{last=$0} END{print last}' )
    if [[ $rc -eq 0 && -n "$version" ]]; then
        RESULTS+=( "PASS  $label  --version=$version" )
        PASS=$(( PASS + 1 ))
    else
        RESULTS+=( "FAIL  $label  rc=$rc out=$out" )
        FAIL=$(( FAIL + 1 ))
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
        "/test/$stem/lba2cc --version"
done

# AppImage verification has a cross-arch limitation: the AppImage type-2
# runtime stub uses syscalls / binary patterns that qemu-user's binfmt
# handler doesn't translate reliably, so an aarch64 AppImage run inside
# an arm64-emulated container exits with "Exec format error" even
# though the file is valid aarch64 ELF (the tarball binary's plain glibc
# code works in the same container, which is what isolates the cause
# to the AppImage runtime stub specifically). Skip cross-arch AppImages
# and verify only AppImages whose arch matches the host.
HOST_ARCH="$(uname -m)"
for aimg in "${APPIMAGES[@]}"; do
    stem="${aimg%.AppImage}"
    case "$aimg" in
        *aarch64*) aimg_arch="aarch64"; platform="linux/arm64" ;;
        *x86_64*)  aimg_arch="x86_64";  platform="linux/amd64" ;;
        *)         aimg_arch="unknown"; platform="linux/amd64" ;;
    esac
    if [[ "$aimg_arch" != "$HOST_ARCH" ]]; then
        RESULTS+=( "SKIP  appimage $stem  cross-arch AppImage (host=$HOST_ARCH, image=$aimg_arch) — qemu-user can't run AppImage runtime stub" )
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
        RESULTS+=( "FAIL  appimage $stem  --appimage-extract failed on host" )
        FAIL=$(( FAIL + 1 ))
        continue
    fi
    run_check \
        "appimage $stem" \
        "$platform" \
        "ro" \
        "/test/$extract_dir/squashfs-root/AppRun --version"
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
