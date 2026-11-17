#!/bin/sh
# Install Sawmill, the AppImage store:
#   curl -fsSL https://raw.githubusercontent.com/kuhwar/sawmill/main/install.sh | sh
#
# Downloads the latest Sawmill AppImage for this computer and runs it once. On that first run
# Sawmill moves itself to ~/.local/share/sawmill/github.com/kuhwar/sawmill/ and adds itself to
# your applications. Running it again updates Sawmill to the latest release.
set -eu

REPO=kuhwar/sawmill

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
TMP=$(mktemp "${TMPDIR:-/tmp}/Sawmill-XXXXXX.AppImage")
trap 'rm -f "$TMP"' EXIT INT TERM

say "Downloading Sawmill for $ARCH..."
if command -v curl >/dev/null 2>&1; then
  curl -fL --progress-bar "$URL" -o "$TMP" || fail "download failed: $URL"
elif command -v wget >/dev/null 2>&1; then
  wget -q --show-progress "$URL" -O "$TMP" || fail "download failed: $URL"
else
  fail "curl or wget is needed to download Sawmill."
fi
chmod +x "$TMP"

DEST="${XDG_DATA_HOME:-$HOME/.local/share}/sawmill/github.com/$REPO"
say "Starting Sawmill. It moves itself to $DEST and adds itself to your applications."
# Run detached; Sawmill moves the file itself, so it must not be deleted here.
trap - EXIT INT TERM
nohup "$TMP" >/dev/null 2>&1 &
say "Done. Next time, open Sawmill from your applications."
