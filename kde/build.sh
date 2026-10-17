#!/bin/bash
# Win12: build and install the patched KDE packages from Arch's own PKGBUILDs (spec part 1), in dependency order.
# usage: kde/build.sh [libplasma] [plasma-workspace] [plasma-desktop]     (default: all three)
# Also builds and installs kde/hook (win12-kde-hook: the pacman hook that says to run this again after a stock Plasma update).
# Needs git, makepkg, gpg. Installing needs `sudo pacman`: run it in a terminal (sudo asks for the password) or
# add a sudoers drop-in for /usr/bin/pacman alone; with neither, the packages are built and the pacman -U command is printed. Undo: sudo pacman -S libplasma plasma-workspace plasma-desktop
# Prints WIN12_BUILD_OK <pkg> or WIN12_BUILD_FAIL <pkg> <step> and stops at the first failure (a patch that no
# longer applies fails in makepkg's prepare(), before anything is installed).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/win12-kde"
PKGS=${*:-libplasma plasma-workspace plasma-desktop}
# in a terminal sudo may ask for the password; unattended runs need the pacman-only sudoers drop-in
if sudo -n /usr/bin/pacman -V >/dev/null 2>&1 || [ -t 0 ]; then INSTALL=1; else INSTALL=; echo "Win12: sudo pacman needs a password: building only"; fi
PKG=; STEP=
step() { STEP=$1; echo "== $PKG: $STEP"; }
trap '[ $? -ne 0 ] && echo "WIN12_BUILD_FAIL ${PKG:-?} ${STEP:-?}"' EXIT
for PKG in $PKGS; do
  step version; VER=$(pacman -Q "$PKG" | cut -d' ' -f2)
  D="$CACHE/$PKG"; rm -rf "$D"; mkdir -p "$CACHE"
  step "clone $VER"; git clone -q --depth 1 --branch "$VER" "https://gitlab.archlinux.org/archlinux/packaging/packages/$PKG.git" "$D"
  step pkgbuild; (cd "$D" && bash "$HERE/pkgbuild-win12.sh" "$PKG" "$HERE/patches/$PKG")
  step "gpg keys"; (cd "$D" && source ./PKGBUILD && gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys "${validpgpkeys[@]}")
  step makepkg; (cd "$D" && makepkg -f ${INSTALL:+-s} --noconfirm)
  if [ -n "$INSTALL" ]; then step install; sudo pacman -U --noconfirm "$D"/*.pkg.tar.zst
  else echo "to install: sudo pacman -U $D/*.pkg.tar.zst"; fi
  echo "WIN12_BUILD_OK $PKG"
done
# the hook last: installed earlier it would warn on every patched package still missing
PKG=win12-kde-hook; D="$CACHE/hook"; rm -rf "$D"; mkdir -p "$D"
step copy; cp "$HERE"/hook/* "$D"
step makepkg; (cd "$D" && makepkg -f --noconfirm)
if [ -n "$INSTALL" ]; then step install; sudo pacman -U --noconfirm "$D"/*.pkg.tar.zst
else echo "to install: sudo pacman -U $D/*.pkg.tar.zst"; fi
echo "WIN12_BUILD_OK $PKG"
