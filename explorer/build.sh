#!/bin/sh
# Win12 Explorer: build the win12-explorer package with makepkg and install it.
# usage: explorer/build.sh [--version|--force]
# Installing needs `sudo pacman` (terminal, or a pacman-only sudoers drop-in); otherwise the pacman -U line is printed.
# Prints WIN12_BUILD_OK win12-explorer or WIN12_BUILD_FAIL win12-explorer <step>.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/win12-explorer"
VER=0.1.$(cd "$HERE" && find . -type f ! -path '*/build/*' | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -c1-12)
[ "${1:-}" = --version ] && { echo "$VER"; exit 0; }
STEP=
step() { STEP=$1; echo "== win12-explorer: $STEP"; }
trap '[ $? -ne 0 ] && echo "WIN12_BUILD_FAIL win12-explorer ${STEP:-?}"' EXIT
if [ "${1:-}" != --force ] && [ "$(pacman -Q win12-explorer 2>/dev/null | cut -d' ' -f2)" = "$VER-1" ]; then
  echo "WIN12_BUILD_OK win12-explorer (installed)"; exit 0
fi
step deps; command -v makepkg >/dev/null || sudo pacman -S --needed --noconfirm base-devel
step source; mkdir -p "$CACHE"; rm -f "$CACHE"/win12-explorer-*.pkg.tar.zst
tar -C "$HERE" --exclude=./build --transform "s,^\.,win12-explorer-$VER," -czf "$CACHE/win12-explorer-$VER.tar.gz" .
sed "s/@VER@/$VER/" "$HERE/PKGBUILD" > "$CACHE/PKGBUILD"
step makepkg; (cd "$CACHE" && makepkg -sfC --noconfirm)
if sudo -n /usr/bin/pacman -V >/dev/null 2>&1 || [ -t 0 ]; then
  step install; sudo pacman -U --noconfirm "$CACHE"/win12-explorer-"$VER"-1-*.pkg.tar.zst
else
  echo "Win12: sudo pacman needs a password: built only"; echo "to install: sudo pacman -U $CACHE/win12-explorer-$VER-1-*.pkg.tar.zst"
fi
echo "WIN12_BUILD_OK win12-explorer"
