#!/bin/sh
# Install Sawmill, the AppImage store:
#   curl -fsSL https://raw.githubusercontent.com/projectlarch/sawmill/main/install.sh | sh
#
# Downloads the latest Sawmill AppImage for this computer to
# ~/.local/share/sawmill/github.com/projectlarch/sawmill/, next to the apps Sawmill installs, adds it to
# your applications and starts it. Running it again updates Sawmill to the latest release.
set -eu

REPO=projectlarch/sawmill

say() { printf '%s\n' "$*"; }
fail() { printf 'Sawmill install: %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = Linux ] || fail "Sawmill only runs on Linux."
[ "$(id -u)" != 0 ] || fail "run this as your normal user, not root; Sawmill installs into your home folder."

case "$(uname -m)" in
  x86_64 | amd64) ARCH=x86_64 ;;
  aarch64 | arm64) ARCH=aarch64 ;;
  *) fail "no Sawmill build for $(uname -m); builds exist for x86_64 and aarch64." ;;
esac

command -v git >/dev/null 2>&1 || fail "Sawmill needs git to download its catalog. Install it first, e.g.:
  sudo apt install git     (Debian, Ubuntu)
  sudo dnf install git     (Fedora)
  sudo pacman -S git       (Arch)"

URL="https://github.com/$REPO/releases/latest/download/Sawmill-$ARCH.AppImage"
ICON_URL="https://raw.githubusercontent.com/$REPO/main/client/data/icons/io.sawmill.Store.png"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
DEST="$DATA/sawmill/github.com/$REPO"
APPIMAGE="$DEST/app.AppImage"
ICON="$DEST/icon.png"
# same file names Sawmill uses for the apps it installs
DESKTOP="$DATA/applications/sawmill-github.com-$(printf '%s' "$REPO" | tr '/' '-').desktop"

# Sawmill moved from kuhwar/sawmill to projectlarch/sawmill: remove an install from the old place
OLD_DEST="$DATA/sawmill/github.com/kuhwar/sawmill"
OLD_DESKTOP="$DATA/applications/sawmill-github.com-kuhwar-sawmill.desktop"
if [ -e "$OLD_DEST" ] || [ -e "$OLD_DESKTOP" ]; then
  say "Removing the Sawmill install from its old location..."
  rm -rf "$OLD_DEST" "$OLD_DESKTOP"
  rmdir "$DATA/sawmill/github.com/kuhwar" 2>/dev/null || true
fi

mkdir -p "$DEST" "$DATA/applications"
PART="$APPIMAGE.part"
trap 'rm -f "$PART"' EXIT INT TERM

say "Downloading Sawmill for $ARCH..."
TAG=
if command -v curl >/dev/null 2>&1; then
  # releases/latest redirects to .../releases/tag/<tag>
  TAG=$(curl -fsI -o /dev/null -w '%{redirect_url}' "https://github.com/$REPO/releases/latest" | sed -n 's|.*/releases/tag/||p') || TAG=
  curl -fL --progress-bar "$URL" -o "$PART" || fail "download failed: $URL"
  curl -fsL "$ICON_URL" -o "$ICON" || true
elif command -v wget >/dev/null 2>&1; then
  wget -q --show-progress "$URL" -O "$PART" || fail "download failed: $URL"
  wget -q "$ICON_URL" -O "$ICON" || true
else
  fail "curl or wget is needed to download Sawmill."
fi
chmod +x "$PART"
mv -f "$PART" "$APPIMAGE"
trap - EXIT INT TERM

# the installed release, like the version file of apps Sawmill installs
if [ -n "$TAG" ]; then printf '%s' "$TAG" > "$DEST/version"; else rm -f "$DEST/version"; fi
[ -s "$ICON" ] || { rm -f "$ICON"; ICON=io.sawmill.Store; }

cat > "$DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=Sawmill
Comment=Find, install and update AppImages
Exec="$APPIMAGE" %U
TryExec=$APPIMAGE
Icon=$ICON
Terminal=false
Categories=GNOME;GTK;System;PackageManager;
Keywords=app;store;software;install;appimage;
StartupNotify=true
StartupWMClass=io.sawmill.Store
X-Sawmill-Id=github.com/$REPO
EOF
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q "$DATA/applications" 2>/dev/null || true
fi

say "Installed Sawmill${TAG:+ $TAG} in $DEST."
nohup "$APPIMAGE" >/dev/null 2>&1 &
say "Sawmill is starting. Next time, open it from your applications."
