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
mv -v ./packaging/lba2cc.png ./AppDir
mv -v ./packaging/lba2cc.desktop ./AppDir

read -r VERSION < ./dist/VERSION.txt
export ARCH VERSION
export OUTPATH=./dist
export ADD_HOOKS="self-updater.hook"
export UPINFO="gh-releases-zsync|${GITHUB_REPOSITORY%/*}|${GITHUB_REPOSITORY#*/}|latest|*$ARCH.AppImage.zsync"
export APPNAME="LBA2 Classic Community"
export DEPLOY_OPENGL=1
          
quick-sharun ./build/SOURCES/lba2
quick-sharun --make-appimage
