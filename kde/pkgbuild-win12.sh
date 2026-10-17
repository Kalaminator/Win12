#!/bin/bash
# Edits the Arch PKGBUILD in the current dir for Win12: our patches in source= and a prepare() that applies them,
# tests on and a check() for our QTests, provides <pkg>-win12. (The pacman hook is its own package, kde/hook.)
# Fails (nothing written) when the PKGBUILD shape changed: fix this script, do not edit the clone by hand.
# usage: pkgbuild-win12.sh <pkg> <patch dir>
set -eu
PKG=$1 PD=$2
grep -q '^prepare()' PKGBUILD && { echo "$PKG: PKGBUILD already has prepare(): merge by hand"; exit 1; }
grep -q -- '-DBUILD_TESTING=OFF' PKGBUILD || { echo "$PKG: no -DBUILD_TESTING=OFF in PKGBUILD"; exit 1; }
sed -i 's/-DBUILD_TESTING=OFF/-DBUILD_TESTING=ON/' PKGBUILD
FILES=()
for f in "$PD"/*.patch; do cp "$f" .; FILES+=("$(basename "$f")"); done
SUMS=(); for f in "${FILES[@]}"; do SUMS+=("'$(sha256sum "$f" | cut -c1-64)'"); done
{
  echo; echo '# --- Win12 (kde/pkgbuild-win12.sh): patches, tests, provides ---'
  echo "source+=(${FILES[*]})"
  echo "sha256sums+=(${SUMS[*]})"
  echo "provides+=(\"$PKG-win12=\$pkgver-\$pkgrel\")"
  echo 'prepare() { cd "$pkgname-$pkgver"; for p in "$srcdir"/*.patch; do patch -Np1 -i "$p"; done; }'
  echo "check() { ctest --test-dir build -R 'win12|coronatest' --output-on-failure; }"
  if [ "$PKG" = plasma-workspace ]; then echo 'makedepends+=(qt6-wayland)  # BUILD_TESTING=ON needs Qt6 WaylandCompositor; Arch does not list it'; fi
} >> PKGBUILD
