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

# Comment this out if you need an AUR package
make-aur-package libsmacker

echo "Making lba2 classic community..."
echo "---------------------------------------------------------------"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target lba2

# Copy the version file to the dist folder
cp build/VERSION.txt ./dist/VERSION.txt

mkdir -p ./AppDir/bin
mv -v ./packaging/lba2.png ./AppDir
mv -v ./packaging/lba2.desktop ./AppDir
mv -v ./packaging/change-working-dir.hook ./AppDir/bin

# quick-sharun
wget "https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/quick-sharun.sh" -O ./quick-sharun
chmod +x ./quick-sharun
          
export ARCH
export OUTPATH=./dist
export ADD_HOOKS="self-updater.hook"
export UPINFO="gh-releases-zsync|${GITHUB_REPOSITORY%/*}|${GITHUB_REPOSITORY#*/}|latest|*$ARCH.AppImage.zsync"
export DEPLOY_OPENGL=1
          
./quick-sharun ./build/SOURCES/lba2
./quick-sharun --make-appimage
