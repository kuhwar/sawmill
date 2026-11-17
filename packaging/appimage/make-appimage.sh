#!/bin/sh
# Build Sawmill and pack it as an AppImage in ./dist with quick-sharun.
# Run get-dependencies.sh first. VERSION defaults to `git describe`.
set -eu

ARCH=$(uname -m)
VERSION=${VERSION:-$(git describe --tags --always 2>/dev/null || echo dev)}
BUILD_DIR=${BUILD_DIR:-/tmp/sawmill-build}
REPO=${GITHUB_REPOSITORY:-kuhwar/sawmill}

rm -rf "$BUILD_DIR" ./AppDir ./dist
meson setup "$BUILD_DIR" client --prefix=/usr --buildtype=release -Drelease_tag="$VERSION"
meson compile -C "$BUILD_DIR"
meson install -C "$BUILD_DIR"

export OUTPATH=./dist
export DESKTOP=/usr/share/applications/io.sawmill.Store.desktop
export ICON=/usr/share/icons/hicolor/512x512/apps/io.sawmill.Store.png
export OUTNAME="Sawmill-$VERSION-$ARCH.AppImage"
# lets AppImage updaters find newer releases of Sawmill itself
export UPINFO="gh-releases-zsync|${REPO%/*}|${REPO#*/}|latest|Sawmill-*-$ARCH.AppImage.zsync"

quick-sharun /usr/bin/sawmill
quick-sharun --make-appimage

ls -la ./dist
