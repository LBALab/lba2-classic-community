#!/usr/bin/env bash
# Bundle a built Android native library into a signed APK.
#
# Usage:
#   bundle-android.sh --lib <path/to/liblba2cc.so> \
#                     --version <version-string> \
#                     --arch <arm64-v8a> \
#                     --build-dir <cmake-build-dir> \
#                     --sdk-root <android-sdk> \
#                     --sdl3-java-src <path-to-sdl3-java-sources> \
#                     --output-dir <where-to-drop-the-apk> \
#                     [--keystore <file> --keystore-pass <pass> \
#                      --key-alias <alias> [--key-pass <pass>]]
#
# Produces: <output-dir>/lba2cc-<version>-android-<arch>.apk
#
# Signing. Android identifies an app by package name AND signing certificate,
# so two APKs signed with different keys are two different apps: installing one
# over the other is refused with INSTALL_FAILED_UPDATE_INCOMPATIBLE, which the
# package installer shows as "App not installed as package conflicts with an
# existing package". The only way past it is to uninstall, and that erases
# everything in the app's private storage.
#
# Pass a keystore (flags above, or LBA2_ANDROID_KEYSTORE / _KEYSTORE_PASS /
# _KEY_ALIAS / _KEY_PASS in the environment) and every release signed with it
# updates in place. With none, this falls back to the local debug keystore,
# generating one if the machine has none -- which is fine for a build you
# install yourself and wrong for anything a player is handed, because a fresh
# CI runner generates a fresh key on every single run.
#
# Requires the Android SDK (command-line tools, build-tools, platform
# android-34) to be installed at SDK_ROOT.
set -euo pipefail

LIB_PATH=""
VERSION=""
ARCH="arm64-v8a"
BUILD_DIR=""
SDK_ROOT=""
SDL3_JAVA_SRC=""
SDL3_LIB=""
CXX_SHARED_LIB=""
OUTPUT_DIR=""
KEYSTORE="${LBA2_ANDROID_KEYSTORE:-}"
KEYSTORE_PASS="${LBA2_ANDROID_KEYSTORE_PASS:-}"
KEY_ALIAS="${LBA2_ANDROID_KEY_ALIAS:-}"
KEY_PASS="${LBA2_ANDROID_KEY_PASS:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --lib) LIB_PATH="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --arch) ARCH="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --sdk-root) SDK_ROOT="$2"; shift 2 ;;
        --sdl3-java-src) SDL3_JAVA_SRC="$2"; shift 2 ;;
        --sdl3-lib) SDL3_LIB="$2"; shift 2 ;;
        --cxx-shared-lib) CXX_SHARED_LIB="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --keystore) KEYSTORE="$2"; shift 2 ;;
        --keystore-pass) KEYSTORE_PASS="$2"; shift 2 ;;
        --key-alias) KEY_ALIAS="$2"; shift 2 ;;
        --key-pass) KEY_PASS="$2"; shift 2 ;;
        -h|--help)
            sed -n '/^# Usage:/,/^set -e/p' "$0" | sed 's/^# \?//' | head -n -1
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

for var in LIB_PATH VERSION BUILD_DIR SDK_ROOT OUTPUT_DIR; do
    if [[ -z "${!var}" ]]; then
        echo "bundle-android.sh: missing required arg: ${var,,}" >&2
        exit 2
    fi
done

if [[ -z "$SDL3_JAVA_SRC" ]]; then
    echo "bundle-android.sh: missing required arg: sdl3-java-src" >&2
    exit 2
fi

if [[ ! -f "$LIB_PATH" ]]; then
    echo "bundle-android.sh: native lib not found at $LIB_PATH" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGE="org.lbalab.lba2cc"
LIB_NAME="libmain.so"
ARTIFACT_NAME="lba2cc-${VERSION}-android-${ARCH}"
ARTIFACT_APK="${OUTPUT_DIR}/${ARTIFACT_NAME}.apk"

# versionCode is the integer Android orders builds by, and the manifest carried
# a hard-coded 1, so every release we have ever shipped claimed to be the same
# build. Derive it from the leading X.Y.Z of the version, ignoring any -dev or
# -rc suffix: 0.12.0 -> 1200, 0.13.0 -> 1300. Two digits each for minor and
# patch, which is room this project will not run out of before the major moves.
VERSION_CODE=$(
    echo "$VERSION" | awk -F'[.-]' '{
        printf "%d", ($1 * 10000) + ($2 * 100) + $3
    }'
)
if [[ ! "$VERSION_CODE" =~ ^[0-9]+$ ]] || [[ "$VERSION_CODE" -le 0 ]]; then
    echo "bundle-android: could not derive a versionCode from '$VERSION'" >&2
    exit 1
fi

echo "[bundle-android] lib:        $LIB_PATH"
echo "[bundle-android] version:    $VERSION (versionCode $VERSION_CODE)"
echo "[bundle-android] arch:       $ARCH"
echo "[bundle-android] artifact:   $ARTIFACT_APK"

# Locate Android SDK tools
BUILD_TOOLS=$(ls -d "$SDK_ROOT/build-tools/"* 2>/dev/null | sort -V | tail -1)
if [[ -z "$BUILD_TOOLS" ]]; then
    echo "bundle-android: no build-tools found under SDK_ROOT/build-tools/" >&2
    exit 1
fi
AAPT="$BUILD_TOOLS/aapt"
AAPT2="$BUILD_TOOLS/aapt2"
ZIPALIGN="$BUILD_TOOLS/zipalign"
APKSIGNER="$BUILD_TOOLS/apksigner"

for tool in "$AAPT" "$ZIPALIGN" "$APKSIGNER"; do
    if [[ ! -x "$tool" ]]; then
        echo "bundle-android: required tool not found: $tool" >&2
        exit 1
    fi
done

# Fresh staging directory
STAGING="${OUTPUT_DIR}/apk-staging"
rm -rf "$STAGING"
mkdir -p "$STAGING/lib/$ARCH"
mkdir -p "$STAGING/res/values"

# 1. Place native libraries
cp "$LIB_PATH" "$STAGING/lib/$ARCH/$LIB_NAME"
# Bundle all SDL3 shared libs from the install prefix
if [[ -n "$SDL3_LIB" ]]; then
    SDL3_LIBDIR="$(dirname "$SDL3_LIB")"
    if [[ -d "$SDL3_LIBDIR" ]]; then
        for lib in "$SDL3_LIBDIR"/*.so; do
            [[ -f "$lib" ]] && cp "$lib" "$STAGING/lib/$ARCH/"
        done
    fi
fi
# Bundle C++ runtime — auto-discover from the NDK via CMakeCache.txt,
# falling back to the explicit --cxx-shared-lib flag if provided.
if [[ -n "$CXX_SHARED_LIB" && -f "$CXX_SHARED_LIB" ]]; then
    cp "$CXX_SHARED_LIB" "$STAGING/lib/$ARCH/"
elif [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    # A cache configured through the NDK toolchain file carries no
    # CMAKE_ANDROID_NDK, and a grep miss under `set -e` would end the run right
    # here with no output at all. Let it through: emptiness is handled below,
    # and the toolchain paths still name the NDK when that variable does not.
    NDK_ROOT=$(grep -m1 '^CMAKE_ANDROID_NDK:' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2- || true)
    ABI=$(grep -m1 '^ANDROID_ABI:' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2- || true)
    if [[ -z "$NDK_ROOT" ]]; then
        NDK_AR=$(grep -m1 '^CMAKE_AR:' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2- || true)
        [[ "$NDK_AR" == */toolchains/* ]] && NDK_ROOT="${NDK_AR%%/toolchains/*}"
    fi
    ABI="${ABI:-$ARCH}"
    if [[ -n "$NDK_ROOT" && -n "$ABI" ]]; then
        case "$ABI" in
            arm64-v8a)   NDK_TARGET="aarch64-linux-android" ;;
            armeabi-v7a) NDK_TARGET="arm-linux-androideabi" ;;
            x86_64)      NDK_TARGET="x86_64-linux-android" ;;
            x86)         NDK_TARGET="i686-linux-android" ;;
        esac
        case "$(uname -s)" in
            Linux)  HOST_TAG="linux-x86_64" ;;
            Darwin) HOST_TAG="darwin-x86_64" ;;
            *)      HOST_TAG="" ;;
        esac
        if [[ -n "$NDK_TARGET" && -n "$HOST_TAG" ]]; then
            CXX_SHARED_LIB="${NDK_ROOT}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot/usr/lib/${NDK_TARGET}/libc++_shared.so"
            if [[ -f "$CXX_SHARED_LIB" ]]; then
                echo "[bundle-android] bundling: $CXX_SHARED_LIB"
                cp "$CXX_SHARED_LIB" "$STAGING/lib/$ARCH/"
            fi
        fi
    fi
fi

# An APK missing a library the native code links against installs cleanly and
# then dies at load time, which is a worse failure than not building at all.
if grep -aq 'libc++_shared\.so' "$LIB_PATH" 2>/dev/null \
   && [[ ! -f "$STAGING/lib/$ARCH/libc++_shared.so" ]]; then
    echo "bundle-android: $LIB_PATH links against libc++_shared.so, which could" >&2
    echo "  not be located from the build directory. Pass it explicitly with" >&2
    echo "  --cxx-shared-lib <path to libc++_shared.so>." >&2
    exit 1
fi

# 1b. App icon — resolvable as @mipmap/ic_launcher in the manifest
ICON_SRC="$REPO_ROOT/packaging/lba2cc.png"
if [[ -f "$ICON_SRC" ]]; then
    mkdir -p "$STAGING/res/mipmap-hdpi"
    cp "$ICON_SRC" "$STAGING/res/mipmap-hdpi/ic_launcher.png"
fi

# 2. AndroidManifest.xml — resolve @string references from the manifest
PRODUCT_NAME="${LBA2_PRODUCT_NAME:-LBA2 Classic Community}"
cat > "$STAGING/res/values/strings.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="app_name">${PRODUCT_NAME}</string>
    <string name="version_name">${VERSION}</string>
</resources>
EOF

cp "$REPO_ROOT/packaging/android/AndroidManifest.xml" "$STAGING/AndroidManifest.xml"

# The manifest keeps a literal versionCode so it stays a valid manifest to read
# and to build by hand; the real value is substituted here. Checked rather than
# assumed: a manifest edit that renamed or reformatted the attribute would
# otherwise ship every build as version 1 again, silently, which is the bug this
# replaces.
# [0-9][0-9]* rather than [0-9]\+: the latter is a GNU extension to basic
# regular expressions, so on a BSD userland it matches a literal + and this
# substitution quietly leaves the manifest saying 1.
if ! grep -q 'android:versionCode="[0-9][0-9]*"' "$STAGING/AndroidManifest.xml"; then
    echo "bundle-android: no android:versionCode to substitute in the manifest" >&2
    exit 1
fi
sed -i.bak "s/android:versionCode=\"[0-9][0-9]*\"/android:versionCode=\"${VERSION_CODE}\"/" \
    "$STAGING/AndroidManifest.xml"
rm -f "$STAGING/AndroidManifest.xml.bak"
# The grep above says an attribute was there to replace, not that the replacing
# happened. Those are different failures and only this one is silent.
if ! grep -q "android:versionCode=\"${VERSION_CODE}\"" "$STAGING/AndroidManifest.xml"; then
    echo "bundle-android: the versionCode substitution did not take" >&2
    exit 1
fi

# 3. Compile SDL Java sources (plus helpers from packaging/android/java/) into classes.dex
echo "[bundle-android] compiling Java to DEX..."
SDL_JAVA_DIR="${SDL3_JAVA_SRC}/android-project/app/src/main/java"
REPO_JAVA_DIR="$REPO_ROOT/packaging/android/java"
D8="$BUILD_TOOLS/d8"
# Gather SDL sources
SDL_JAVA_FILES=""
if [[ -d "$SDL_JAVA_DIR" ]]; then
    SDL_JAVA_FILES=$(find "$SDL_JAVA_DIR" -name '*.java' 2>/dev/null | tr '\n' ' ')
fi
# Gather repo-local helpers
REPO_JAVA_FILES=""
if [[ -d "$REPO_JAVA_DIR" ]]; then
    REPO_JAVA_FILES=$(find "$REPO_JAVA_DIR" -name '*.java' 2>/dev/null | tr '\n' ' ')
fi
# Merge (strip leading/trailing whitespace for -n check)
ALL_JAVA_FILES="${SDL_JAVA_FILES} ${REPO_JAVA_FILES}"
ALL_JAVA_FILES="${ALL_JAVA_FILES#"${ALL_JAVA_FILES%%[![:space:]]*}"}"
ALL_JAVA_FILES="${ALL_JAVA_FILES%"${ALL_JAVA_FILES##*[![:space:]]}"}"
if [[ -n "$ALL_JAVA_FILES" && -x "$D8" ]]; then
    mkdir -p "$STAGING/obj"
    javac -d "$STAGING/obj" \
        -classpath "$SDK_ROOT/platforms/android-34/android.jar" \
        $ALL_JAVA_FILES 2>&1
    mapfile -t CLASS_FILES < <(find "$STAGING/obj" -name '*.class')
    "$D8" --lib "$SDK_ROOT/platforms/android-34/android.jar" \
        --output "$STAGING" "${CLASS_FILES[@]}" 2>&1
    rm -rf "$STAGING/obj"
fi

# 4. Build APK with aapt
echo "[bundle-android] packaging APK..."
"$AAPT" package -f -M "$STAGING/AndroidManifest.xml" \
    -S "$STAGING/res" \
    -I "$SDK_ROOT/platforms/android-34/android.jar" \
    -F "$STAGING/unsigned.apk" 2>&1

# 5. Add classes.dex and native lib to the APK (store .so uncompressed)
echo "[bundle-android] adding native lib and DEX..."
cd "$STAGING"
if [[ -f classes.dex ]]; then
    "$AAPT" add "unsigned.apk" "classes.dex" 2>&1
fi
for lib in "lib/$ARCH/"*.so; do
    [[ -f "$lib" ]] && "$AAPT" add -0 '' "unsigned.apk" "$lib" 2>&1
done
cd "$REPO_ROOT"

# 6. Zipalign (-P 16: align uncompressed .so to 16 KB pages for 16 KB-page devices)
echo "[bundle-android] aligning..."
"$ZIPALIGN" -f -P 16 4 "$STAGING/unsigned.apk" "$STAGING/aligned.apk"

# 7. Sign — see the header for why the identity of the key matters
if [[ -n "$KEYSTORE" ]]; then
    if [[ ! -f "$KEYSTORE" ]]; then
        echo "bundle-android: keystore not found at $KEYSTORE" >&2
        exit 1
    fi
    if [[ -z "$KEYSTORE_PASS" || -z "$KEY_ALIAS" ]]; then
        echo "bundle-android: --keystore needs --keystore-pass and --key-alias" >&2
        exit 1
    fi
    # A key with no separate password uses the store's, which is how a keystore
    # holding one key is usually made.
    KEY_PASS="${KEY_PASS:-$KEYSTORE_PASS}"
    echo "[bundle-android] signing with the release key ($KEY_ALIAS)"
else
    KEYSTORE="${HOME}/.android/debug.keystore"
    KEYSTORE_PASS="android"
    KEY_ALIAS="androiddebugkey"
    KEY_PASS="android"
    if [[ ! -f "$KEYSTORE" ]]; then
        echo "[bundle-android] generating debug keystore..."
        mkdir -p "${HOME}/.android"
        keytool -genkey -v -keystore "$KEYSTORE" \
            -alias "$KEY_ALIAS" -storepass "$KEYSTORE_PASS" -keypass "$KEY_PASS" \
            -keyalg RSA -keysize 2048 -validity 10000 \
            -dname "CN=Android Debug,O=Android,C=US" 2>&1
    fi
    echo "[bundle-android] signing with the local debug key."
    echo "[bundle-android] NOTE: this APK can only update another one signed by"
    echo "[bundle-android]       the same machine's debug key. Do not publish it."
fi

# pass: puts the password in the argument list, and on Linux that is world
# readable through /proc/<pid>/cmdline for as long as the process runs. env: is
# the same string somewhere only this user can read it.
export LBA2_APKSIGNER_STORE_PASS="$KEYSTORE_PASS"
export LBA2_APKSIGNER_KEY_PASS="$KEY_PASS"
"$APKSIGNER" sign --ks "$KEYSTORE" \
    --ks-pass env:LBA2_APKSIGNER_STORE_PASS \
    --ks-key-alias "$KEY_ALIAS" \
    --key-pass env:LBA2_APKSIGNER_KEY_PASS \
    --out "$ARTIFACT_APK" "$STAGING/aligned.apk"
unset LBA2_APKSIGNER_STORE_PASS LBA2_APKSIGNER_KEY_PASS

# 8. Verify, and print the certificate.
#
# The digest is the app's identity as far as Android is concerned, so a release
# log that carries it is how anyone can check afterwards that two builds really
# can update each other -- which is exactly the question nobody could answer
# about the releases signed by throwaway keys.
echo "[bundle-android] verifying..."
"$APKSIGNER" verify --print-certs "$ARTIFACT_APK" 2>&1 |
    grep -i "certificate SHA-256 digest" || true
"$APKSIGNER" verify "$ARTIFACT_APK" 2>&1

# 9. Cleanup
rm -rf "$STAGING"

APK_SIZE=$(du -h "$ARTIFACT_APK" | cut -f1)
echo "[bundle-android] done: $ARTIFACT_APK ($APK_SIZE)"
