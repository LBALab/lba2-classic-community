#!/usr/bin/env bash
# Bundle a built Android native library into a debug-signed APK.
#
# Usage:
#   bundle-android.sh --lib <path/to/liblba2cc.so> \
#                     --version <version-string> \
#                     --arch <arm64-v8a> \
#                     --build-dir <cmake-build-dir> \
#                     --sdk-root <android-sdk> \
#                     --sdl3-java-src <path-to-sdl3-java-sources> \
#                     --output-dir <where-to-drop-the-apk>
#
# Produces: <output-dir>/lba2cc-<version>-android-<arch>.apk
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

echo "[bundle-android] lib:        $LIB_PATH"
echo "[bundle-android] version:    $VERSION"
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
    NDK_ROOT=$(grep -m1 '^CMAKE_ANDROID_NDK:' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2-)
    ABI=$(grep -m1 '^ANDROID_ABI:' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2-)
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

cp "$REPO_ROOT/android/AndroidManifest.xml" "$STAGING/AndroidManifest.xml"

# 3. Compile SDL Java sources (plus helpers from android/src/) into classes.dex
echo "[bundle-android] compiling Java to DEX..."
SDL_JAVA_DIR="${SDL3_JAVA_SRC}/android-project/app/src/main/java"
REPO_JAVA_DIR="$REPO_ROOT/android/src"
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
    "$D8" --lib "$SDK_ROOT/platforms/android-34/android.jar" \
        --output "$STAGING" $(find "$STAGING/obj" -name '*.class') 2>&1
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
    [[ -f "$lib" ]] && "$AAPT" add -0 .so "unsigned.apk" "$lib" 2>&1
done
cd "$REPO_ROOT"

# 6. Zipalign
echo "[bundle-android] aligning..."
"$ZIPALIGN" -f -p 4 "$STAGING/unsigned.apk" "$STAGING/aligned.apk"

# 7. Debug-sign with the auto-generated debug keystore
KEYSTORE="${HOME}/.android/debug.keystore"
KEYPASS="android"
if [[ ! -f "$KEYSTORE" ]]; then
    echo "[bundle-android] generating debug keystore..."
    mkdir -p "${HOME}/.android"
    keytool -genkey -v -keystore "$KEYSTORE" \
        -alias androiddebugkey -storepass "$KEYPASS" -keypass "$KEYPASS" \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US" 2>&1
fi

echo "[bundle-android] signing..."
"$APKSIGNER" sign --ks "$KEYSTORE" \
    --ks-pass "pass:${KEYPASS}" \
    --key-pass "pass:${KEYPASS}" \
    --out "$ARTIFACT_APK" "$STAGING/aligned.apk"

# 8. Verify
echo "[bundle-android] verifying..."
"$APKSIGNER" verify "$ARTIFACT_APK" 2>&1

# 9. Cleanup
rm -rf "$STAGING"

APK_SIZE=$(du -h "$ARTIFACT_APK" | cut -f1)
echo "[bundle-android] done: $ARTIFACT_APK ($APK_SIZE)"
