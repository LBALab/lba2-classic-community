#!/usr/bin/env bash
# Pack the UWP exe from build-uwp.sh into a signed .appx for an Xbox in Developer Mode.
# Usage: bash scripts/dev/package-uwp.sh [BUILD]   (BUILD 0-65535, default 0)
# The package version is the build's version (VERSION.txt) with BUILD as its fourth part.
# Signs with UWP_SIGN_CERT and UWP_SIGN_KEY (PEM text) when set, else with a throwaway cert.
# Output: out/uwp/lba2cc-<VERSION.txt>-xbox-x64.appx
set -euo pipefail

if [ "$#" -gt 1 ] || { [ "$#" -eq 1 ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; }; then
    sed -n '2,6s/^# //p' "$0"
    [ "$#" -eq 1 ] && exit 0
    exit 2
fi
BUILD_NUMBER="${1:-0}"
if ! [[ "$BUILD_NUMBER" =~ ^[0-9]+$ ]] || [ "$BUILD_NUMBER" -gt 65535 ]; then
    echo "BUILD must be a number from 0 to 65535, got $BUILD_NUMBER" >&2
    exit 2
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$REPO_DIR/out/build/uwp-ci"
EXE="$BUILD/SOURCES/lba2cc.exe"
OUT="$REPO_DIR/out/uwp"
STAGE="$OUT/stage"

[ -f "$EXE" ] || { echo "$EXE missing; run scripts/dev/build-uwp.sh first" >&2; exit 1; }
[ -f "$BUILD/VERSION.txt" ] || { echo "$BUILD/VERSION.txt missing" >&2; exit 1; }
command -v osslsigncode >/dev/null || { echo "osslsigncode not found" >&2; exit 1; }

# 0.13.3-dev names the file; 0.13.3 plus BUILD is the package version. Windows
# updates a package in place only to a higher version, so a rolling build
# passes a number that rises. Each part is a 16-bit number.
VERSION_TEXT="$(tr -d '[:space:]' <"$BUILD/VERSION.txt")"
if ! [[ "$VERSION_TEXT" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+) ]]; then
    echo "VERSION.txt does not start with a.b.c: $VERSION_TEXT" >&2
    exit 1
fi
PACKAGE_VERSION="${BASH_REMATCH[1]}.${BASH_REMATCH[2]}.${BASH_REMATCH[3]}.$BUILD_NUMBER"
for part in "${BASH_REMATCH[@]:1:3}"; do
    [ "$part" -le 65535 ] || { echo "version part $part is over 65535" >&2; exit 1; }
done
APPX="$OUT/lba2cc-$VERSION_TEXT-xbox-x64.appx"

rm -rf "$OUT" && mkdir -p "$STAGE"
cp "$EXE" "$REPO_DIR/tools/uwp/AppxManifest.xml" "$STAGE/"
cp -R "$REPO_DIR/tools/uwp/Assets" "$STAGE/"
python3 -I - "$STAGE/AppxManifest.xml" "$PACKAGE_VERSION" <<'EOF'
import re, sys
path, version = sys.argv[1], sys.argv[2]
text = open(path, encoding="utf-8").read()
text, n = re.subn(r'(<Identity\b[^>]*\bVersion=")[^"]*(")', r"\g<1>" + version + r"\2", text)
if n != 1:
    sys.exit(f"{path}: expected one Identity Version, found {n}")
open(path, "w", encoding="utf-8").write(text)
EOF
echo "Package version $PACKAGE_VERSION"

KEYS="$OUT/keys"
mkdir -p "$KEYS" && chmod 700 "$KEYS"
trap 'rm -rf "$KEYS"' EXIT
if [ -n "${UWP_SIGN_CERT:-}" ] && [ -n "${UWP_SIGN_KEY:-}" ]; then
    printf '%s\n' "$UWP_SIGN_CERT" >"$KEYS/cert.pem"
    (umask 077 && printf '%s\n' "$UWP_SIGN_KEY" >"$KEYS/key.pem")
    echo "Signing with the cert from UWP_SIGN_CERT"
else
    # The subject must match the manifest's Publisher.
    openssl req -x509 -newkey rsa:2048 -nodes -keyout "$KEYS/key.pem" -out "$KEYS/cert.pem" \
        -days 30 -subj "/CN=lba2cc-dev" -addext extendedKeyUsage=codeSigning \
        -addext keyUsage=digitalSignature -addext basicConstraints=critical,CA:FALSE 2>/dev/null
    echo "Signing with a throwaway cert (UWP_SIGN_CERT not set)"
fi
publisher="$(sed -n 's/.*<Identity[^>]*Publisher="\([^"]*\)".*/\1/p' "$STAGE/AppxManifest.xml")"
subject="$(openssl x509 -in "$KEYS/cert.pem" -noout -subject -nameopt RFC2253 | sed 's/^subject=//')"
if [ "$subject" != "$publisher" ]; then
    echo "cert subject $subject does not match the manifest's Publisher $publisher" >&2
    exit 1
fi

python3 -I "$REPO_DIR/tools/uwp/pack_appx.py" "$STAGE" "$OUT/unsigned.appx"
osslsigncode sign -certs "$KEYS/cert.pem" -key "$KEYS/key.pem" -in "$OUT/unsigned.appx" -out "$APPX" >/dev/null
osslsigncode verify -in "$APPX" -CAfile "$KEYS/cert.pem" >/dev/null
rm -f "$OUT/unsigned.appx"
echo "$APPX"
