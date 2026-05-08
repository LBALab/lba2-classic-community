#!/bin/sh
set -eu
ARCH=$(uname -m)

echo "Installing package dependencies..."
echo "---------------------------------------------------------------"
pacman -Syu --noconfirm \
    cmake    \
    libdecor \
    sdl3

echo "Installing debloated packages..."
echo "---------------------------------------------------------------"
get-debloated-pkgs --add-common --prefer-nano

echo "Making lba2 classic community..."
echo "---------------------------------------------------------------"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target lba2

# Copy the version file to dist directory
mkdir -p ./dist
cp build/VERSION.txt ./dist/VERSION.txt

mkdir -p ./AppDir/bin
mv -v ./packaging/lba2.png ./AppDir
mv -v ./packaging/lba2.desktop ./AppDir
mv -v ./packaging/change-working-dir.hook ./AppDir/bin

# SECURE FETCH OF QUICK-SHARUN
# Pin to a specific commit and verify SHA-256 for reproducibility
QUICK_SHARUN_COMMIT="92aac234ece00dee3af438d278793313eee8fb4d"
QUICK_SHARUN_URL="https://githubusercontent.com{QUICK_SHARUN_COMMIT}/useful-tools/quick-sharun.sh"
QUICK_SHARUN_SHA256="d44d51f25152a5b2aa8c9ed5c2cceb24cef4331b4cd4e2a12ff2f0f47728bf77"

echo "Fetching and verifying quick-sharun..."
curl -fsSL "$QUICK_SHARUN_URL" -o ./quick-sharun
echo "${QUICK_SHARUN_SHA256}  ./quick-sharun" | sha256sum -c -
chmod +x ./quick-sharun

VERSION=$(cat ./dist/VERSION.txt)
export ARCH VERSION
export OUTPATH=./dist
export ADD_HOOKS="self-updater.hook"
export UPINFO="gh-releases-zsync|${GITHUB_REPOSITORY%/*}|${GITHUB_REPOSITORY#*/}|latest|*$ARCH.AppImage.zsync"
export APPNAME="LBA2 Classic Community"
export DEPLOY_OPENGL=1
          
./quick-sharun ./build/SOURCES/lba2
./quick-sharun --make-appimage
