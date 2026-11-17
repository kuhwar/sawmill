#!/bin/sh
# Build dependencies for the Sawmill AppImage. Runs as root in the
# ghcr.io/pkgforge-dev/archlinux container (see .github/workflows/appimage.yml).
set -eu

# Anylinux helper scripts, pinned to a reviewed commit (bump deliberately).
ANYLINUX_COMMIT=64fb5d4487a9c524773b2dc611cdf6d018c767a4
ANYLINUX="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/$ANYLINUX_COMMIT/useful-tools"

pacman -Syu --noconfirm --needed \
  base-devel git meson ninja wget patchelf strace xorg-server-xvfb \
  gtk4 libadwaita libsoup3 json-glib glib2 \
  librsvg webp-pixbuf-loader

# Swap in smaller builds of common libraries (must run after every pacman -S).
wget -q "$ANYLINUX/get-debloated-pkgs.sh" -O /tmp/get-debloated-pkgs.sh
sh /tmp/get-debloated-pkgs.sh --add-common --prefer-nano

wget -q "$ANYLINUX/quick-sharun.sh" -O /usr/local/bin/quick-sharun
chmod +x /usr/local/bin/quick-sharun
